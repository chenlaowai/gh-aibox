#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#include <cmath>

#include <boost/beast/websocket.hpp>

#include "common.hpp"
#include "task.hpp"
#include <sail/tensor.h>

namespace aidetect {

// ---------------------------------------------------------------------------
// WebSocket session + hub
// ---------------------------------------------------------------------------
class StreamHub;

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    using SubCallback = std::function<void(const std::string&)>;
    WsSession(beast::websocket::stream<beast::tcp_stream>&& ws, StreamHub& hub,
              SubCallback on_sub, SubCallback on_unsub)
        : ws_(std::move(ws)), hub_(hub), on_sub_(std::move(on_sub)),
          on_unsub_(std::move(on_unsub)) {}
    ~WsSession() {
        stop();
        if (sender_.joinable()) sender_.join();
    }

    void run() {
        sender_ = std::thread([this] { sender_loop(); });
        beast::flat_buffer buffer;
        while (!stopped_) {
            beast::error_code ec;
            ws_.read(buffer, ec);
            if (ec) break;
            std::string msg = beast::buffers_to_string(buffer.data());
            buffer.consume(buffer.size());
            if (msg == "heartbeat") continue;
            subscribe(msg);
        }
        stop();
    }

    void send(const std::shared_ptr<std::vector<unsigned char>>& packet) {
        {
            std::lock_guard<std::mutex> lk(q_m_);
            if (stopped_) return;
            if (q_.size() >= 2) q_.pop_front();
            q_.push_back(packet);
        }
        cv_.notify_one();
    }

    std::string task_id() {
        std::lock_guard<std::mutex> lk(m_);
        return task_id_;
    }

    void stop() {
        std::string old_id;
        {
            std::lock_guard<std::mutex> lk(m_);
            if (subscribed_) {
                old_id = task_id_;
                subscribed_ = false;
                task_id_.clear();
                if (!old_id.empty() && on_unsub_) on_unsub_(old_id);
            }
        }
        {
            std::lock_guard<std::mutex> lk(q_m_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

private:
    void subscribe(const std::string& id) {
        std::string old_id;
        {
            std::lock_guard<std::mutex> lk(m_);
            if (stopped_) return;
            old_id = task_id_;
            task_id_ = id;
            subscribed_ = true;
            if (!old_id.empty() && old_id != id && on_unsub_) on_unsub_(old_id);
            if (old_id != id && on_sub_) on_sub_(id);
        }
        if (old_id != id) log_info("ws subscribe: " + id);
    }

    void sender_loop() {
        while (true) {
            std::shared_ptr<std::vector<unsigned char>> packet;
            {
                std::unique_lock<std::mutex> lk(q_m_);
                cv_.wait(lk, [this] { return stopped_ || !q_.empty(); });
                if (stopped_ && q_.empty()) break;
                packet = q_.front();
                q_.pop_front();
            }
            try {
                ws_.binary(true);
                ws_.write(asio::buffer(*packet));
            } catch (const std::exception& e) {
                log_warn(std::string("ws write: ") + e.what());
                stop();
                break;
            }
        }
    }

    beast::websocket::stream<beast::tcp_stream> ws_;
    StreamHub& hub_;
    SubCallback on_sub_;
    SubCallback on_unsub_;
    std::mutex m_;
    std::string task_id_;
    bool subscribed_ = false;
    std::mutex q_m_;
    std::condition_variable cv_;
    std::deque<std::shared_ptr<std::vector<unsigned char>>> q_;
    std::atomic<bool> stopped_{false};
    std::thread sender_;
};

class StreamHub {
public:
    void add(const std::shared_ptr<WsSession>& s) {
        std::lock_guard<std::mutex> lk(m_);
        sessions_.push_back(s);
    }
    void remove(const std::shared_ptr<WsSession>& s) {
        std::lock_guard<std::mutex> lk(m_);
        sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                       [&](const std::weak_ptr<WsSession>& w) {
                                           auto p = w.lock();
                                           return !p || p == s;
                                       }),
                        sessions_.end());
    }
    // Preview is produced on demand (same idea as the vendor's
    // DecodeInstance::requireScaled/revokeScaled): a channel is only encoded
    // and sent when a browser actually watches it or its 3x3 merge group.
    void note_subscribe(const std::string& id) {
        std::lock_guard<std::mutex> lk(sub_m_);
        ++subs_[id];
        log_info("preview demand +1: " + id + " -> " + std::to_string(subs_[id]));
    }
    void note_unsubscribe(const std::string& id) {
        std::lock_guard<std::mutex> lk(sub_m_);
        auto it = subs_.find(id);
        if (it == subs_.end()) return;
        if (--it->second <= 0) {
            subs_.erase(it);
            log_info("preview demand -1: " + id + " -> 0");
        }
    }
    bool wanted(const std::string& id) {
        std::lock_guard<std::mutex> lk(sub_m_);
        auto it = subs_.find(id);
        return it != subs_.end() && it->second > 0;
    }
    void publish(const std::string& task_id, const std::vector<unsigned char>& jpeg) {
        Json header = {{"taskId", task_id},
                       {"imageType", "jpeg"},
                       {"ImageType", "jpeg"},
                       {"dataLength", jpeg.size()}};
        std::string h = header.dump();
        auto packet = std::make_shared<std::vector<unsigned char>>();
        packet->reserve(4 + h.size() + jpeg.size());
        uint32_t n = static_cast<uint32_t>(h.size());
        packet->push_back(static_cast<unsigned char>((n >> 24) & 0xff));
        packet->push_back(static_cast<unsigned char>((n >> 16) & 0xff));
        packet->push_back(static_cast<unsigned char>((n >> 8) & 0xff));
        packet->push_back(static_cast<unsigned char>(n & 0xff));
        packet->insert(packet->end(), h.begin(), h.end());
        packet->insert(packet->end(), jpeg.begin(), jpeg.end());
        std::vector<std::shared_ptr<WsSession>> alive;
        {
            std::lock_guard<std::mutex> lk(m_);
            for (auto it = sessions_.begin(); it != sessions_.end();) {
                auto s = it->lock();
                if (!s) {
                    it = sessions_.erase(it);
                } else {
                    alive.push_back(s);
                    ++it;
                }
            }
        }
        for (auto& s : alive) {
            if (s->task_id() == task_id) s->send(packet);
        }
    }

private:
    std::mutex m_;
    std::vector<std::weak_ptr<WsSession>> sessions_;
    std::mutex sub_m_;
    std::map<std::string, int> subs_;
};

// ---------------------------------------------------------------------------
// 4 x (3x3) aggregate compositor
// ---------------------------------------------------------------------------
class Aggregator {
public:
    explicit Aggregator(StreamHub& hub) : hub_(hub) {
        if (std::getenv("AIDETECT_DISABLE_AGG")) {
            log_warn("aggregator disabled by AIDETECT_DISABLE_AGG");
            return;
        }
        for (int g = 0; g < 4; ++g) {
            groups_[g].thread = std::thread([this, g] { loop(g); });
            groups_[g].send_thread = std::thread([this, g] { send_loop(g); });
        }
    }
    ~Aggregator() {
        stop_ = true;
        for (auto& g : groups_) {
            g.cv.notify_all();
            g.out_cv.notify_all();
        }
        for (auto& g : groups_) {
            if (g.thread.joinable()) g.thread.join();
            if (g.send_thread.joinable()) g.send_thread.join();
        }
    }
    void update(int group, int slot, const std::vector<unsigned char>& jpeg) {
        if (group < 0 || group >= 4 || slot < 0 || slot >= 9) return;
        auto p = std::make_shared<std::vector<unsigned char>>(jpeg);
        std::lock_guard<std::mutex> lk(groups_[group].m);
        groups_[group].slots[slot] = p;
        groups_[group].dirty = true;
        groups_[group].cv.notify_one();
    }
    // Drop a stopped task's last frame so slot reuse cannot show a stale
    // camera/overlay from the previous task.
    void clear(int group, int slot) {
        if (group < 0 || group >= 4 || slot < 0 || slot >= 9) return;
        std::lock_guard<std::mutex> lk(groups_[group].m);
        groups_[group].slots[slot].reset();
        groups_[group].dirty = true;
        groups_[group].cv.notify_one();
    }

private:
    struct Group {
        std::mutex m;
        std::condition_variable cv;
        std::array<std::shared_ptr<std::vector<unsigned char>>, 9> slots;
        // Composer-thread-only cache: decoded 640x360 tile + the JPEG pointer it
        // was decoded from. Avoids re-decoding unchanged/status tiles.
        std::array<std::shared_ptr<std::vector<unsigned char>>, 9> cached_src;
        std::array<std::shared_ptr<sail::BMImage>, 9> cached_img;
        std::thread thread;
        std::thread send_thread;
        std::shared_ptr<std::vector<unsigned char>> latest;
        std::mutex out_mu;
        std::condition_variable out_cv;
        bool has_new = false;
        bool dirty = false;
    };

    void loop(int g) {
        sail::Handle handle(0);
        sail::Bmcv bmcv(handle);
        cv::Mat black_cv = cv::Mat::zeros(360, 640, CV_8UC3);
        cv::Mat no_task = cv::imread("/app/no_task.jpg");
        if (!no_task.empty()) {
            cv::resize(no_task, black_cv, cv::Size(640, 360));
        }
        // Everything the merge needs lives on the device: each tile is decoded
        // by the JPU, copied into the BGR canvas by VPP and the 1080p canvas is
        // encoded by the JPU. This mirrors the vendor MergeInstance + JPEG
        // encoder design and keeps the CPU free for decoding/inference.
        sail::BMImage black_bm = sail::Bmcv::mat_to_bm_image(black_cv);
        sail::BMImage canvas(handle, 1080, 1920, FORMAT_BGR_PACKED,
                             DATA_TYPE_EXT_1N_BYTE);
        bool hw_ok = true;
        bool was_active = false;
        int64_t last_compose = 0;
        while (!stop_) {
            const std::string key = "all_" + std::to_string(g + 1);
            if (!hub_.wanted(key)) {
                was_active = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            std::array<std::shared_ptr<std::vector<unsigned char>>, 9> cur;
            {
                std::unique_lock<std::mutex> lk(groups_[g].m);
                if (!was_active) {
                    // A browser just subscribed: compose and send the current
                    // grid once even if all tiles are static.
                    was_active = true;
                    groups_[g].dirty = true;
                }
                groups_[g].cv.wait_for(lk, std::chrono::milliseconds(200),
                                       [&] { return stop_ || groups_[g].dirty; });
                if (stop_) break;
                if (!groups_[g].dirty) continue;
                groups_[g].dirty = false;
                cur = groups_[g].slots;
            }
            int64_t now = now_ms();
            int64_t wait = 100 - (now - last_compose);
            if (wait > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait));
                now = now_ms();
            }
            // Slot start is recorded after pacing, so encode time cannot make the
            // merge run faster than 10fps (the next slot is at least 100ms away).
            last_compose = now;
            std::vector<unsigned char> buf;
            if (hw_ok) {
                try {
                    std::lock_guard<std::mutex> lk(jpeg_hw_mutex());
                    auto& grp = groups_[g];
                    for (int i = 0; i < 9; ++i) {
                        int x = (i % 3) * 640;
                        int y = (i / 3) * 360;
                        if (!cur[i] || cur[i]->empty()) {
                            grp.cached_src[i].reset();
                            grp.cached_img[i].reset();
                            bmcv.image_copy_to(black_bm, canvas, x, y);
                            continue;
                        }
                        // Static/status tiles repeat for seconds; decoding them
                        // on every merge frame was the main cost. Decode only
                        // when the tile bytes actually changed.
                        if (!grp.cached_img[i] || grp.cached_src[i] != cur[i]) {
                            if (!jpeg_markers_ok(*cur[i])) {
                                // Truncated tile: keep the previous image and
                                // retry when new bytes arrive (KI-030).
                                if (!grp.cached_img[i])
                                    bmcv.image_copy_to(black_bm, canvas, x, y);
                                else
                                    bmcv.image_copy_to(*grp.cached_img[i], canvas, x, y);
                                continue;
                            }
                            sail::BMImage img =
                                bmcv.imdecode(cur[i]->data(), cur[i]->size());
                            if (img.width() <= 0 || img.height() <= 0) {
                                // Decode failed: keep the last good tile rather
                                // than flashing black/colour noise.
                                grp.cached_src[i] = cur[i];
                                if (grp.cached_img[i])
                                    bmcv.image_copy_to(*grp.cached_img[i], canvas, x, y);
                                else
                                    bmcv.image_copy_to(black_bm, canvas, x, y);
                                continue;
                            }
                            if (img.width() == 640 && img.height() == 360) {
                                grp.cached_img[i] =
                                    std::make_shared<sail::BMImage>(std::move(img));
                            } else {
                                sail::BMImage r = bmcv.resize(
                                    img, 640, 360,
                                    bmcv_resize_algorithm::BMCV_INTER_LINEAR);
                                grp.cached_img[i] =
                                    std::make_shared<sail::BMImage>(std::move(r));
                            }
                            grp.cached_src[i] = cur[i];
                        }
                        if (grp.cached_img[i])
                            bmcv.image_copy_to(*grp.cached_img[i], canvas, x, y);
                        else
                            bmcv.image_copy_to(black_bm, canvas, x, y);
                    }
                    std::string ext = ".jpg";
                    if (!bmcv.imencode(ext, canvas, buf)) buf.clear();
                } catch (const std::exception& e) {
                    log_warn("aggregate hw: " + std::string(e.what()));
                    buf.clear();
                    hw_ok = false;
                }
            }
            if (buf.empty()) {
                // CPU fallback (or no hardware path): compose with OpenCV.
                cv::Mat canvas_cv(1080, 1920, CV_8UC3, cv::Scalar(0, 0, 0));
                for (int i = 0; i < 9; ++i) {
                    cv::Mat tile = black_cv;
                    if (cur[i] && !cur[i]->empty()) {
                        try {
                            cv::Mat img = cv::imdecode(*cur[i], cv::IMREAD_COLOR);
                            if (!img.empty()) {
                                if (img.cols != 640 || img.rows != 360)
                                    cv::resize(img, tile, cv::Size(640, 360));
                                else
                                    tile = img;
                            }
                        } catch (const std::exception& e) {
                            log_warn("aggregate imdecode: " + std::string(e.what()));
                        }
                    }
                    tile.copyTo(canvas_cv(cv::Rect((i % 3) * 640, (i / 3) * 360,
                                                   640, 360)));
                }
                if (!cv::imencode(".jpg", canvas_cv, buf)) {
                    log_warn("aggregate imencode failed");
                    continue;
                }
            }
            {
                std::lock_guard<std::mutex> lk(groups_[g].out_mu);
                groups_[g].latest =
                    std::make_shared<std::vector<unsigned char>>(buf);
                groups_[g].has_new = true;
            }
            groups_[g].out_cv.notify_one();
        }
    }

    void send_loop(int g) {
        const std::string key = "all_" + std::to_string(g + 1);
        while (!stop_) {
            std::shared_ptr<std::vector<unsigned char>> b;
            {
                std::unique_lock<std::mutex> lk(groups_[g].out_mu);
                groups_[g].out_cv.wait_for(
                    lk, std::chrono::milliseconds(200),
                    [&] { return stop_ || groups_[g].has_new; });
                if (stop_) break;
                if (!groups_[g].has_new || !groups_[g].latest) continue;
                b = groups_[g].latest;
                groups_[g].has_new = false;
            }
            if (b && !b->empty()) hub_.publish(key, *b);
        }
    }

    StreamHub& hub_;
    std::array<Group, 4> groups_;
    std::atomic<bool> stop_{false};
};

// ---------------------------------------------------------------------------
// metrics
// ---------------------------------------------------------------------------
inline int read_temp_c(const char* path) {
    std::ifstream f(path);
    long milli = 0;
    if (!(f >> milli)) return 0;
    if (milli > 1000) milli /= 1000;
    if (milli < -40 || milli > 150) return 0;
    return static_cast<int>(milli);
}

inline Json algorithm_catalog() {
    auto p = [](const char* code, const char* name, const char* type, double def,
                const char* desc) {
        return Json{{"code", code}, {"name", name}, {"type", type},
                    {"default", def}, {"desc", desc}};
    };
    Json algs = Json::array();
    algs.push_back({{"code", "head_count"}, {"name", "人数监控（人头）"},
                    {"model", "head"}, {"target", "Head"},
                    {"params", Json::array({p("conf_thresh", "置信度", "float", 0.3, "人头检测置信度"),
                                            p("nms_thresh", "NMS", "float", 0.7, "NMS 阈值"),
                                            p("exceed_num", "触发人数", "int", 1, "人数达到该值触发")})}});
    algs.push_back({{"code", "people_gather"}, {"name", "人员聚集"},
                    {"model", "people"}, {"target", "People"},
                    {"params", Json::array({p("conf_thresh", "置信度", "float", 0.3, "人形检测置信度"),
                                            p("nms_thresh", "NMS", "float", 0.7, "NMS 阈值"),
                                            p("exceed_num", "聚集人数", "int", 3, "人数达到该值触发")})}});
    algs.push_back({{"code", "helmet"}, {"name", "安全帽检测"},
                    {"model", "helmet"}, {"target", "未戴安全帽"},
                    {"params", Json::array({p("conf_thresh", "置信度", "float", 0.3, "人形基座置信度"),
                                            p("nms_thresh", "NMS", "float", 0.7, "NMS 阈值"),
                                            p("exceed_num", "触发人数", "int", 1, "未戴安全帽人数"),
                                            p("attr_thresh", "告警阈值", "float", 0.8, "生产默认 0.8")})}});
    algs.push_back({{"code", "vest"}, {"name", "反光衣检测"},
                    {"model", "helmet"}, {"target", "未穿反光衣"},
                    {"params", Json::array({p("conf_thresh", "置信度", "float", 0.3, "人形基座置信度"),
                                            p("nms_thresh", "NMS", "float", 0.7, "NMS 阈值"),
                                            p("exceed_num", "触发人数", "int", 1, "未穿反光衣人数"),
                                            p("attr_thresh", "告警阈值", "float", 0.9, "生产默认 0.9")})}});
    algs.push_back({{"code", "smoking_call"}, {"name", "抽烟打电话检测"},
                    {"model", "behavior"}, {"target", "抽烟/打电话"},
                    {"params", Json::array({p("conf_thresh", "置信度", "float", 0.4, "生产默认 0.4"),
                                            p("nms_thresh", "NMS", "float", 0.7, "NMS 阈值"),
                                            p("exceed_num", "触发人数", "int", 1, "行为人数")})}});
    return Json{{"status", "ok"}, {"algorithms", algs}};
}
inline Json get_occupancy() {
    Json j;
    try {
        auto s1 = read_cpu_sample();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto s2 = read_cpu_sample();
        auto dt = s2.total - s1.total;
        auto di = s2.idle - s1.idle;
        int cpu = dt > 0 ? static_cast<int>((1.0 - static_cast<double>(di) / dt) * 100) : 0;
        int tpu = sail::get_tpu_util(0);
        auto vpu = sail::get_vpu_util(0);
        auto vpp = sail::get_vpp_util(0);
        auto dev = sail::get_dev_stat(0);
        int mem_pct = read_mem_percent();
        double disk = disk_free_gb("/data");
        int chip = 0, board = 0;
        try { chip = sail::get_chip_temp(0); } catch (...) {}
        try { board = sail::get_board_temp(0); } catch (...) {}
        // The SDK may return 0 when the thermal device is not bound; read the
        // standard Linux hwmon/thermal sysfs as a fallback.
        if (chip <= 0) chip = read_temp_c("/sys/class/hwmon/hwmon0/temp1_input");
        if (board <= 0) board = read_temp_c("/sys/class/thermal/thermal_zone0/temp");
        int dev_used = 0;
        if (dev.size() >= 2 && dev[0] > 0) dev_used = static_cast<int>(dev[1] * 100 / dev[0]);
        auto pct = [](int v) {
            if (v < 0 || v > 100) return 0;
            return v;
        };
        j = {{"status", "ok"},
             {"msg", "返回资源占用率值"},
             {"cpu", pct(cpu)},
             {"mem", pct(mem_pct)},
             {"disk_free", std::round(disk * 100) / 100},
             {"tpu", pct(tpu)},
             {"vpu_1", vpu.size() > 0 ? pct(vpu[0]) : 0},
             {"vpu_2", vpu.size() > 1 ? pct(vpu[1]) : 0},
             {"vpu_3", vpu.size() > 2 ? pct(vpu[2]) : 0},
             {"vpp_1", vpp.size() > 0 ? pct(vpp[0]) : 0},
             {"vpp_2", vpp.size() > 1 ? pct(vpp[1]) : 0},
             {"memory", pct(dev_used)},
             {"chip_temp", chip},
             {"board_temp", board}};
    } catch (const std::exception& e) {
        j = {{"status", "error"}, {"msg", std::string("返回资源占用率值失败: ") + e.what()}};
    }
    return j;
}

// ---------------------------------------------------------------------------
// URL / query
// ---------------------------------------------------------------------------
struct RequestTarget {
    std::string path;
    std::map<std::string, std::string> query;
};

inline RequestTarget parse_target(const std::string& target) {
    RequestTarget out;
    size_t q = target.find('?');
    out.path = q == std::string::npos ? target : target.substr(0, q);
    if (q == std::string::npos) return out;
    std::string qs = target.substr(q + 1);
    size_t start = 0;
    while (start <= qs.size()) {
        size_t amp = qs.find('&', start);
        std::string part = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        size_t eq = part.find('=');
        if (eq != std::string::npos) {
            std::string k = part.substr(0, eq);
            std::string v = part.substr(eq + 1);
            // minimal percent decode
            std::string dv;
            for (size_t i = 0; i < v.size(); ++i) {
                if (v[i] == '%' && i + 2 < v.size()) {
                    auto hex = [](char c) {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        return 0;
                    };
                    dv.push_back(static_cast<char>(hex(v[i + 1]) * 16 + hex(v[i + 2])));
                    i += 2;
                } else {
                    dv.push_back(v[i] == '+' ? ' ' : v[i]);
                }
            }
            out.query[k] = dv;
        }
        if (amp == std::string::npos) break;
        start = amp + 1;
    }
    return out;
}

inline Json parse_json_or_null(const std::string& body) {
    try { return Json::parse(body); } catch (...) { return Json(); }
}

// ---------------------------------------------------------------------------
// task manager + HTTP server
// ---------------------------------------------------------------------------
class TaskManager {
public:
    TaskManager(AppConfig cfg, ModelScheduler& model, DecodePool& decode_pool,
                StreamHub& hub, Aggregator& agg, Jwt jwt)
        : cfg_(std::move(cfg)), model_(model), decode_pool_(decode_pool),
          hub_(hub), agg_(agg), jwt_(std::move(jwt)) {}

    Json start(const std::string& task_id) {
        Json resp;
        if (tasks_.count(task_id)) {
            return {{"status", "ok"}, {"msg", "任务 " + task_id + " 已经在运行中"}};
        }
        TaskConfig tc;
        try {
            tc = fetch_config(task_id);
            cfg_sig_[task_id] = config_signature(tc);
        } catch (const std::exception& e) {
            return {{"status", "error"}, {"msg", std::string("拉取任务配置失败: ") + e.what()}};
        }
        int group = 0, slot = 0;
        if (!assign_slot(task_id, group, slot)) {
            return {{"status", "error"}, {"msg", "没有可用的合流槽位"}};
        }
        auto task = std::make_shared<Task>(
            tc, group, slot, cfg_, model_, decode_pool_,
            [this](const std::string& id, const std::vector<unsigned char>& jpeg) {
                hub_.publish(id, jpeg);
            },
            [this](const std::string& id, int g, int s,
                   const std::vector<unsigned char>& jpeg) {
                agg_.update(g, s, jpeg);
            },
            [this, task_id]() { return hub_.wanted(task_id); },
            [this, group]() {
                return hub_.wanted("all_" + std::to_string(group + 1));
            },
            [this](const std::string& id, const Json& report) { report_alarm(id, report); });
        {
            std::lock_guard<std::mutex> lk(m_);
            tasks_[task_id] = task;
        }
        task->start();
        log_info("task started: " + task_id + " group=" + std::to_string(group) +
                 " slot=" + std::to_string(slot));
        return {{"status", "ok"},
                {"msg", "任务 " + task_id + " 已启动"},
                {"pid", static_cast<int>(getpid())}};
    }

    Json stop(const std::string& task_id) {
        std::shared_ptr<Task> task;
        {
            std::lock_guard<std::mutex> lk(m_);
            auto it = tasks_.find(task_id);
            if (it == tasks_.end()) return {{"status", "ok"}, {"msg", "任务 " + task_id + " 已停止"}};
            task = it->second;
        }
        int group = task->group();
        int slot = task->slot();
        task->stop();
        agg_.clear(group, slot);
        // A decoder read can block indefinitely even after the gateway is
        // killed. Do the join in a detached cleanup thread so the HTTP stop
        // request always returns; the Task shared_ptr keeps the object alive.
        std::thread([task]() { task->join(); }).detach();
        bool group_idle = false;
        {
            std::lock_guard<std::mutex> lk(m_);
            used_.erase({group, slot});
            tasks_.erase(task_id);
            group_idle = true;
            for (auto& kv : tasks_) {
                if (kv.second->group() == group) {
                    group_idle = false;
                    break;
                }
            }
        }
        const char* mode = std::getenv("AIDETECT_DECODE_MODE");
        if (group_idle && mode && std::string(mode) == "multi")
            decode_pool_.reset_group(group);
        log_info("task stopped: " + task_id);
        return {{"status", "ok"}, {"msg", "任务 " + task_id + " 已停止"}};
    }

    Json list() {
        std::lock_guard<std::mutex> lk(m_);
        Json running = Json::array();
        for (auto& kv : tasks_) running.push_back(kv.second->status());
        return {{"running", running}};
    }

    Json scheduler_stats() { return model_.stats(); }

    // After a container/process restart, tasks that the UI still shows as
    // running must be restored. The database is the source of truth.
    static std::string config_signature(const TaskConfig& tc) {
        std::ostringstream s;
        for (auto& m : tc.models) {
            s << m.name << ':' << m.conf << ',' << m.conf_no_helmet << ','
              << m.keep_sec << ',' << m.repeat_sec << ',' << m.attr_thresh
              << ',' << m.people_conf << ',' << (m.people_deep ? 1 : 0)
              << ',' << m.exceed << ',' << m.nms << ',' << m.sensitivity
              << ',' << m.calling_keep_sec << ',' << m.playing_keep_sec
              << ',' << m.smoking_keep_sec << ',' << m.smoking_thresh
              << ',' << m.calling_thresh << ',' << m.phone_thresh
              << ',' << m.cluster_expand << ',' << m.cluster_people
              << ',' << (m.behavior_enable_smoking ? 1 : 0)
              << ',' << (m.behavior_enable_calling ? 1 : 0)
              << ',' << (m.behavior_enable_playing ? 1 : 0)
              << ';';
            // ROI edits are persisted to tb_task_area by the .NET API but the
            // running Task only reads taskArea at construction.  Include the
            // normalized polygons in the config signature so the existing 10s
            // poll restarts the task and immediately starts drawing the new
            // region line (14K saved an ROI but preview had no green line).
            for (auto& poly : m.polygons) {
                for (auto& p : poly) s << p.first << ',' << p.second << ';';
                s << '|';
            }
        }
        return s.str();
    }

    void reconcile_from_backend() {
        if (!cfg_.config_file.empty()) return;
        try {
            std::string url = cfg_.config_base +
                              "/aibox/api/Task/GetTaskPage?PageIndex=1&PageSize=200";
            std::map<std::string, std::string> headers{
                {"Authorization", "Bearer " + jwt_.create("admin")}};
            auto r = HttpClient::get(url, headers, 5000);
            if (r.status != 200) {
                log_warn("task reconcile: http " + std::to_string(r.status));
                return;
            }
            Json root = Json::parse(r.body);
            if (!root.contains("data") || !root["data"].contains("list")) return;
            int started = 0, failed = 0;
            std::vector<Json> pending;
            for (auto& item : root["data"]["list"]) {
                if (item.value("status", 0) == 1 && !item.value("id", "").empty())
                    pending.push_back(item);
            }
            // ISC has a limited number of simultaneous preview sessions. Start
            // the five channels the customer watches first so they win a
            // session before the other 25 tasks race for the remaining slots.
            // All channels are equal. Production does not special-case a
            // preferred 7; ISC previewURL reuse is what keeps 36 sessions
            // alive, so do not bias startup order here.
            auto is_priority = [](const std::string&) { return false; };
            std::stable_sort(pending.begin(), pending.end(),
                             [&](const Json& a, const Json& b) {
                                 return is_priority(a.value("code", "")) &&
                                        !is_priority(b.value("code", ""));
                             });
            for (auto& item : pending) {
                std::string id = item.value("id", "");
                // Periodic reconcile re-enters this loop every 10s.  Already
                // running tasks must not pay the 1.2s ISC stagger again, or
                // the config poll interval would stretch to ~40s.
                if (is_running(id)) continue;
                auto resp = start(id);
                if (resp.value("status", "") == "ok") ++started; else ++failed;
                if (resp.value("status", "") != "ok")
                    log_warn("task start failed: " + id + " " +
                             resp.value("msg", ""));
                // ISC previewURL creation is rate-limited. Starting 30 tasks
                // back-to-back caused a burst of DESCRIBE 500 and only ~11
                // sessions; stagger every task equally.
                std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            }
            if (started || failed)
                log_info("task reconcile: started=" + std::to_string(started) +
                         " failed=" + std::to_string(failed));
            // Algorithm config edits in the Web UI only update the DB. Poll
            // running tasks every 10s and restart the ones whose parameters
            // changed, otherwise a new threshold never reaches the process.
            std::vector<std::string> ids;
            {
                std::lock_guard<std::mutex> lk(m_);
                for (auto& kv : tasks_) ids.push_back(kv.first);
            }
            std::vector<std::pair<std::string, TaskConfig>> changed;
            for (auto& id : ids) {
                try {
                    auto tc = fetch_config(id);
                    auto sig = config_signature(tc);
                    std::lock_guard<std::mutex> lk(m_);
                    auto it = cfg_sig_.find(id);
                    if (it == cfg_sig_.end()) {
                        cfg_sig_[id] = sig;
                    } else if (it->second != sig) {
                        it->second = sig;
                        changed.emplace_back(id, std::move(tc));
                    }
                } catch (...) {
                }
            }
            for (auto& kv : changed) {
                std::shared_ptr<Task> task;
                {
                    std::lock_guard<std::mutex> lk(m_);
                    auto it = tasks_.find(kv.first);
                    if (it != tasks_.end()) task = it->second;
                }
                if (!task) continue;
                // Production TaskChannel::archiveUpdated applies the new task
                // archive in place (TaskInstance::taskArchiveApply); do the
                // same for ROI/algorithm params.  Only a source/name change
                // still needs a real restart.
                if (task->can_hot_update(kv.second)) {
                    task->update_config(kv.second);
                    log_info("task config changed, hot update: " + kv.first);
                } else {
                    log_info("task source/name changed, restarting: " + kv.first);
                    stop(kv.first);
                    start(kv.first);
                }
            }
        } catch (const std::exception& e) {
            log_warn(std::string("task reconcile failed: ") + e.what());
        }
    }

    bool is_running(const std::string& task_id) {
        std::lock_guard<std::mutex> lk(m_);
        return tasks_.count(task_id) != 0;
    }

    static std::string shell_quote(const std::string& s) {
        std::string q = "'";
        for (char c : s) {
            if (c == '\'') q += "'\\''";
            else q += c;
        }
        q += "'";
        return q;
    }

    // Production MediaChannel owns the video stream independently of a task
    // (mediaChannels map, MediaChannel::startMediaIfStopped/healthy), and the
    // channel page reads stored video status/resolution even when no task is
    // configured.  Do not run a full MediaChannel/decoder per idle channel on
    // this box: probe with ffmpeg -c copy for at most 6s, cache the result,
    // and never touch the BPU.  This gives the same status/resolution display
    // with negligible hardware cost.
    Json upload_media(const std::string& original_name, const std::string& body) {
        if (body.empty())
            return {{"status", "error"}, {"msg", "文件为空"}};
        std::string name = std::filesystem::path(original_name).filename().string();
        if (name.empty()) name = "upload.mp4";
        for (auto& c : name) {
            if (!std::isalnum(static_cast<unsigned char>(c)) &&
                c != '.' && c != '_' && c != '-')
                c = '_';
        }
        std::string ext = std::filesystem::path(name).extension().string();
        if (ext.empty()) name += ".mp4";
        std::string dir = cfg_.data_root + "/media";
        ensure_dir(dir);
        std::string path = dir + "/" + std::to_string(now_ms()) + "_" + name;
        std::ofstream f(path, std::ios::binary);
        if (!f)
            return {{"status", "error"}, {"msg", "无法写入文件"}};
        f.write(body.data(), static_cast<std::streamsize>(body.size()));
        f.close();
        log_info("media uploaded: " + path + " bytes=" + std::to_string(body.size()));
        return {{"status", "ok"}, {"path", path}, {"url", path},
                {"size", static_cast<int64_t>(body.size())}};
    }

    Json probe_channel(const std::string& address) {
        if (address.empty())
            return {{"status", "error"}, {"msg", "address 不能为空"}};
        {
            std::lock_guard<std::mutex> lk(probe_mu_);
            auto it = probe_cache_.find(address);
            if (it != probe_cache_.end() &&
                now_ms() - it->second.ts < 60000)
                return it->second.json;
        }
        std::string url = address;
        std::string err;
        try {
            if (url.rfind("isc://", 0) == 0)
                url = IscResolver::resolve(address);
        } catch (const std::exception& e) {
            err = std::string("ISC: ") + e.what();
        }
        std::string resolution;
        if (err.empty()) {
            std::string cmd = "timeout 6 ffmpeg -nostdin -hide_banner -loglevel info ";
            if (url.rfind("rtsp://", 0) == 0)
                cmd += "-rtsp_transport tcp ";
            cmd += "-i " + shell_quote(url) +
                   " -map 0:v:0 -c copy -f null - 2>&1";
            FILE* p = popen(cmd.c_str(), "r");
            std::string out;
            if (p) {
                char buf[2048];
                while (fgets(buf, sizeof(buf), p)) {
                    if (out.size() < 32768) out += buf;
                }
                pclose(p);
            }
            std::smatch m;
            std::regex re(R"(Video:.*?([0-9]{2,5})x([0-9]{2,5}))");
            if (std::regex_search(out, m, re))
                resolution = m[1].str() + "x" + m[2].str();
            if (resolution.empty()) {
                if (out.find("Input/output error") != std::string::npos ||
                    out.find("I/O error") != std::string::npos)
                    err = "Input/output error";
                else if (out.find("404") != std::string::npos ||
                         out.find("not found") != std::string::npos)
                    err = "Camera not found";
                else if (!out.empty())
                    err = out.substr(0, 160);
                else
                    err = "Input/output error";
            }
        }
        Json j;
        if (!resolution.empty()) {
            j = {{"status", "ok"}, {"health", "ok"},
                 {"resolution", resolution}, {"lastError", ""}};
        } else {
            j = {{"status", "error"}, {"health", "error"},
                 {"resolution", ""}, {"lastError", err}};
        }
        {
            std::lock_guard<std::mutex> lk(probe_mu_);
            probe_cache_[address] = {now_ms(), j};
        }
        return j;
    }

    // Production TaskChannel/TaskInstance have healthy()/checkRunCondition;
    // sail::Decoder::read() can stay blocked forever after a gateway death,
    // so the in-Task watchdog is not enough.  Recreate the whole task when no
    // frame arrived for 2 minutes, with a 10-minute per-task backoff.  This
    // is the path that recovers the five ISC channels that previously stayed
    // at "已断开 5xxxx s" until a manual stop/start.
    void supervise_stalled_tasks(int64_t threshold_ms = 120000,
                                 int64_t backoff_ms = 600000) {
        const int64_t now = now_ms();
        std::pair<std::string, int64_t> worst;
        {
            std::lock_guard<std::mutex> lk(m_);
            // One task at a time, at most once per 30s.  Restarting a whole
            // batch at 75s created an ISC session storm: healthy channels
            // were killed while their gateways were still resolving, and the
            // 500/I-O failures cascaded.  Production's MediaChannel::
            // mediaReconnect is per-channel and rate-limited too.
            if (now - last_supervisor_action_ms_ < 30000) return;
            for (auto& kv : tasks_) {
                // Recover streams that worked before, and also tasks that
                // never framed but stopped making progress (wedged thread).
                // A task that never framed is normally an ISC 500/offline
                const int64_t stuck_ms =
                    std::getenv("AIDETECT_STUCK_TASK_MS")
                        ? std::atoll(std::getenv("AIDETECT_STUCK_TASK_MS"))
                        : 900000;
                const bool wedged = kv.second->lastProgressAgeMs() > stuck_ms;
                if (!kv.second->everFrame() && !wedged) continue;
                int64_t age =
                    wedged ? kv.second->lastProgressAgeMs()
                           : kv.second->lastFrameAgeMs();
                if (age < threshold_ms) continue;
                auto it = last_supervisor_restart_ms_.find(kv.first);
                if (it != last_supervisor_restart_ms_.end() &&
                    now - it->second < backoff_ms)
                    continue;
                if (age > worst.second) worst = {kv.first, age};
            }
            if (worst.first.empty()) return;
            last_supervisor_action_ms_ = now;
            last_supervisor_restart_ms_[worst.first] = now;
        }
        log_warn("stream supervisor: " + worst.first + " no frame for " +
                 std::to_string(worst.second) + "ms, restarting task");
        stop(worst.first);
        auto resp = start(worst.first);
        if (resp.value("status", "") != "ok")
            log_warn("stream supervisor restart failed: " + worst.first +
                     " " + resp.value("msg", ""));
    }

    // Event-driven counterpart of the 2s config poll: the Web UI calls this
    // immediately after saving a task/ROI, matching production's
    // handleRemoteMessage -> onTaskArrived -> archiveUpdated path.
    Json reload_config(const std::string& task_id) {
        auto tc = fetch_config(task_id);
        std::shared_ptr<Task> task;
        {
            std::lock_guard<std::mutex> lk(m_);
            auto it = tasks_.find(task_id);
            if (it != tasks_.end()) task = it->second;
        }
        if (!task) return {{"status", "error"}, {"msg", "任务未运行"}};
        if (!task->can_hot_update(tc))
            return {{"status", "error"}, {"msg", "源地址/任务名变化，需要重启任务"}};
        task->update_config(tc);
        {
            std::lock_guard<std::mutex> lk(m_);
            cfg_sig_[task_id] = config_signature(tc);
        }
        log_info("task config changed, direct hot update: " + task_id);
        return {{"status", "ok"}, {"msg", "配置热更新已触发"}};
    }

    Json debug_trigger(const std::string& task_id) {
        std::shared_ptr<Task> task;
        {
            std::lock_guard<std::mutex> lk(m_);
            auto it = tasks_.find(task_id);
            if (it == tasks_.end()) return {{"status", "error"}, {"msg", "任务不存在"}};
            task = it->second;
        }
        task->debug_trigger();
        return {{"status", "ok"}, {"msg", "debug alarm triggered"}};
    }

    Json area_frame(const std::string& task_id) {
        std::shared_ptr<Task> task;
        {
            std::lock_guard<std::mutex> lk(m_);
            auto it = tasks_.find(task_id);
            if (it == tasks_.end()) {
                return {{"status", "error"},
                        {"msg", "任务 " + task_id + " 的配置区域图片不存在"},
                        {"frame_area", nullptr}};
            }
            task = it->second;
        }
        // Ask the decode thread for a fresh full-size frame (it is otherwise
        // skipped when only the merge grid is being watched), then give it a
        // couple of frame periods to land.
        task->mark_area_request();
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        // Raw camera frame: the ROI editor must not inherit preview boxes,
        // labels, task OSD or status graphics.
        auto jpeg = task->area_jpeg();
        if (!jpeg || jpeg->empty()) {
            return {{"status", "error"},
                    {"msg", "任务 " + task_id + " 的配置区域图片不存在"},
                    {"frame_area", nullptr}};
        }
        std::string b64 = base64_encode(jpeg->data(), jpeg->size());
        return {{"status", "ok"},
                {"msg", "返回任务 " + task_id + " 的配置区域图片"},
                {"frame_area", b64}};
    }

private:
    TaskConfig fetch_config(const std::string& task_id) {
        Json root;
        if (!cfg_.config_file.empty()) {
            auto body = read_file(cfg_.config_file);
            if (body.empty()) throw std::runtime_error("config file empty: " + cfg_.config_file);
            root = Json::parse(body);
            // Test/deployment convenience: a single file may hold several task
            // configs keyed by taskId ({"tasks": {"<id>": {...}}}).
            if (root.contains("tasks") && root["tasks"].is_object() &&
                root["tasks"].contains(task_id)) {
                root = root["tasks"][task_id];
            }
        } else {
            std::string url = cfg_.config_base +
                              "/aibox/api/ai/TaskAlgoConfigDetail?task_id=" + url_encode(task_id);
            std::map<std::string, std::string> headers{
                {"Authorization", "Bearer " + jwt_.create("admin")}};
            auto r = HttpClient::get(url, headers, 5000);
            if (r.status != 200) throw std::runtime_error("http " + std::to_string(r.status));
            root = Json::parse(r.body);
            if (root.contains("code") && root["code"] != 200)
                throw std::runtime_error("backend code " + root["code"].dump());
        }
        auto tc = parse_task_config(root);
        if (tc.task_id.empty() || !cfg_.config_file.empty()) tc.task_id = task_id;
        if (tc.channel_address.empty()) throw std::runtime_error("channelAddress empty");
        return tc;
    }

    bool assign_slot(const std::string& task_id, int& group, int& slot) {
        (void)task_id;
        // Production semantics: the first started task takes the first free
        // cell, then the next one, ...; a stopped task leaves its cell as
        // "任务未配置" and later starts fill that hole.  Never hash-scatter and
        // never compact live tasks when one stops.
        std::lock_guard<std::mutex> lk(m_);
        for (int g = 0; g < 4; ++g) {
            for (int s = 0; s < 9; ++s) {
                if (!used_.count({g, s})) {
                    group = g;
                    slot = s;
                    // Reserve while holding m_ so two concurrent starts cannot
                    // pick the same hole.
                    used_.insert({g, s});
                    return true;
                }
            }
        }
        return false;
    }

    void report_alarm(const std::string& task_id, const Json& report) {
        std::string url = cfg_.report_base + "/aibox/api/ai/ReportAlarmEvent";
        std::map<std::string, std::string> headers{
            {"Authorization", "Bearer " + jwt_.create("admin")}};
        auto r = HttpClient::post(url, report.dump(), headers, 5000);
        if (r.status >= 200 && r.status < 300) {
            log_info("alarm reported: " + task_id);
        } else {
            log_error("alarm report failed: status=" + std::to_string(r.status) + " body=" + r.body);
        }
    }

    AppConfig cfg_;
    ModelScheduler& model_;
    DecodePool& decode_pool_;
    StreamHub& hub_;
    Aggregator& agg_;
    Jwt jwt_;
    std::mutex m_;
    std::map<std::string, std::shared_ptr<Task>> tasks_;
    std::set<std::pair<int, int>> used_;
    std::map<std::string, std::string> cfg_sig_;
    struct ProbeEntry {
        int64_t ts = 0;
        Json json;
    };
    std::mutex probe_mu_;
    std::map<std::string, ProbeEntry> probe_cache_;
    std::map<std::string, int64_t> last_supervisor_restart_ms_;
    int64_t last_supervisor_action_ms_ = 0;

};

class HttpServer {
public:
    HttpServer(AppConfig cfg, Jwt jwt, TaskManager& manager, StreamHub& hub)
        : cfg_(std::move(cfg)),
          jwt_(std::move(jwt)),
          manager_(manager),
          hub_(hub),
          ioc_(),
          acceptor_(ioc_, tcp::endpoint(tcp::v4(), cfg_.port)) {
        acceptor_.set_option(asio::socket_base::reuse_address(true));
    }

    void run() {
        log_info("HTTP/WS listening on 0.0.0.0:" + std::to_string(cfg_.port));
        while (true) {
            tcp::socket socket(ioc_);
            beast::error_code ec;
            acceptor_.accept(socket, ec);
            if (ec) {
                log_warn("accept: " + ec.message());
                continue;
            }
            std::thread([this, s = std::move(socket)]() mutable {
                handle_connection(std::move(s));
            }).detach();
        }
    }

private:
    void handle_connection(tcp::socket socket) {
        beast::error_code ec;
        beast::flat_buffer buffer;
        http::request_parser<http::string_body> parser;
        // MP4 tests are uploaded as a raw body; Beast's default 1 MB body
        // limit is far too small for a short clip.
        parser.body_limit(512ull * 1024ull * 1024ull);
        http::read(socket, buffer, parser, ec);
        if (ec) return;
        auto req = parser.release();
        auto target = parse_target(std::string(req.target()));
        if (beast::websocket::is_upgrade(req)) {
            handle_ws(std::move(socket), req, target);
            return;
        }
        if (req.method() == http::verb::options) {
            send_json(socket, 200, {{"status", "ok"}});
            return;
        }
        auto auth = authorize(req);
        if (!auth) {
            send_json(socket, 401, {{"status", "error"}, {"msg", "Token 校验失败"}});
            return;
        }
        Json body_json;
        if (!req.body().empty()) body_json = parse_json_or_null(req.body());
        Json resp;
        int status = 200;
        try {
            if (target.path == "/algorithm/list") {
                resp = algorithm_catalog();
            } else if (target.path == "/get_occupancy") {
                resp = get_occupancy();
            } else if (target.path == "/task/start" && req.method() == http::verb::post) {
                std::string task_id = body_json.value("taskId", "");
                if (task_id.empty()) {
                    status = 400;
                    resp = {{"status", "error"}, {"msg", "taskId 不能为空"}};
                } else {
                    resp = manager_.start(task_id);
                    // The .NET backend only sees a failed StartTaskAsync when
                    // the AI endpoint returns a non-2xx status.  Returning 200
                    // with {"status":"error"} made it mark a 37th task as
                    // Running even though no slot existed.
                    if (resp.value("status", "") != "ok") status = 409;
                }
            } else if (target.path == "/task/stop" && req.method() == http::verb::post) {
                std::string task_id = body_json.value("taskId", "");
                resp = manager_.stop(task_id);
            } else if (target.path == "/task/reload_config" &&
                       req.method() == http::verb::post) {
                std::string task_id = body_json.value("taskId", "");
                if (task_id.empty()) {
                    status = 400;
                    resp = {{"status", "error"}, {"msg", "taskId 不能为空"}};
                } else {
                    resp = manager_.reload_config(task_id);
                    if (resp.value("status", "") != "ok") status = 409;
                }
            } else if (target.path == "/channel/upload" &&
                       req.method() == http::verb::post) {
                std::string name = "upload.mp4";
                auto qit = target.query.find("name");
                if (qit != target.query.end()) name = qit->second;
                resp = manager_.upload_media(name, req.body());
            } else if (target.path == "/channel/probe" &&
                       req.method() == http::verb::post) {
                std::string address = body_json.value("address", "");
                if (address.empty()) {
                    status = 400;
                    resp = {{"status", "error"}, {"msg", "address 不能为空"}};
                } else {
                    resp = manager_.probe_channel(address);
                }
            } else if (target.path == "/task/list") {
                resp = manager_.list();
            } else if (target.path == "/debug/trigger_alarm") {
                if (!std::getenv("AIDETECT_DEBUG")) {
                    status = 404;
                    resp = {{"status", "error"}, {"msg", "not found"}};
                } else {
                    std::string task_id = body_json.value("taskId", "");
                    if (task_id.empty()) {
                        auto qit = target.query.find("taskId");
                        if (qit != target.query.end()) task_id = qit->second;
                    }
                    resp = manager_.debug_trigger(task_id);
                }
            } else if (target.path == "/debug/scheduler") {
                if (!std::getenv("AIDETECT_DEBUG")) {
                    status = 404;
                    resp = {{"status", "error"}, {"msg", "not found"}};
                } else {
                    resp = {{"status", "ok"}, {"instances", manager_.scheduler_stats()}};
                }
            } else if (target.path == "/task/get_area_frame") {
                std::string task_id;
                auto qit = target.query.find("taskId");
                if (qit != target.query.end()) task_id = qit->second;
                resp = manager_.area_frame(task_id);
            } else if (target.path == "/health") {
                resp = {{"status", "ok"}};
            } else {
                status = 404;
                resp = {{"status", "error"}, {"msg", "not found"}};
            }
        } catch (const std::exception& e) {
            status = 500;
            resp = {{"status", "error"}, {"msg", e.what()}};
        }
        send_json(socket, status, resp);
    }

    void handle_ws(tcp::socket socket, http::request<http::string_body>& req,
                   const RequestTarget& target) {
        std::string token;
        auto qit = target.query.find("token");
        if (qit != target.query.end()) token = qit->second;
        auto payload = jwt_.verify(token);
        if (!payload) {
            send_json(socket, 401, {{"status", "error"}, {"msg", "Token 校验失败"}});
            return;
        }
        beast::websocket::stream<beast::tcp_stream> ws(std::move(socket));
        beast::error_code ec;
        ws.accept(req, ec);
        if (ec) return;
        auto session = std::make_shared<WsSession>(
            std::move(ws), hub_,
            [this](const std::string& id) { hub_.note_subscribe(id); },
            [this](const std::string& id) { hub_.note_unsubscribe(id); });
        hub_.add(session);
        session->run();
    }

    bool authorize(const http::request<http::string_body>& req) {
        auto it = req.find(http::field::authorization);
        if (it == req.end()) return false;
        std::string v = std::string(it->value());
        const std::string prefix = "Bearer ";
        if (v.rfind(prefix, 0) != 0) return false;
        return jwt_.verify(v.substr(prefix.size())).has_value();
    }

    void send_json(tcp::socket& socket, int status, const Json& body) {
        http::response<http::string_body> res{static_cast<http::status>(status), 11};
        res.set(http::field::server, "aidetect");
        res.set(http::field::content_type, "application/json");
        // Web UI (port 80) calls the AI control port 9000 directly after a
        // task/ROI save.  Keep the token auth, just allow the cross-origin
        // request from the same box.
        res.set("Access-Control-Allow-Origin", "*");
        res.set("Access-Control-Allow-Headers", "Authorization, Content-Type");
        res.set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.keep_alive(false);
        res.body() = body.dump();
        res.prepare_payload();
        beast::error_code ec;
        http::write(socket, res, ec);
        socket.shutdown(tcp::socket::shutdown_send, ec);
    }

    AppConfig cfg_;
    Jwt jwt_;
    TaskManager& manager_;
    StreamHub& hub_;
    asio::io_context ioc_;
    tcp::acceptor acceptor_;
};

}  // namespace aidetect
