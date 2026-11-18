#pragma once
#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>
#include "common.hpp"
#include "alarm_recorder.hpp"
#include "ingest_bridge.hpp"
#include "ts_segment_recorder.hpp"
#include "decode_pool.hpp"
#include "isc.hpp"
#include "model.hpp"
#include "routeb/compat.hpp"
#include <spawn.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
extern char** environ;
namespace aidetect {
struct Detection {
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    float score = 0;
    int class_id = -1;
    bool target = false;
    bool pending = false;  // target detected, not yet confirmed -> blue box
    bool confirmed = false; // per-target countdown passed -> red box
    bool suppress_label = false; // confirmed but inside report interval
    float vx = 0, vy = 0;  // px/s, extrapolate between detections
    int countdown_s = 0;    // blue countdown seconds
    std::string label;
};
using PreviewCallback = std::function<void(const std::string& task_id, int group, int slot,
                                           const std::vector<unsigned char>& jpeg)>;
using SinglePreviewCallback = std::function<void(const std::string& task_id,
                                                 const std::vector<unsigned char>& jpeg)>;
using DemandCallback = std::function<bool()>;
using AlarmCallback = std::function<void(const std::string& task_id,
                                         const Json& report)>;
class Task {
public:
    Task(TaskConfig cfg, int group, int slot, AppConfig app, ModelScheduler& scheduler,
         DecodePool& decode_pool, SinglePreviewCallback single_cb,
         PreviewCallback tile_cb, DemandCallback want_single,
         DemandCallback want_tile, AlarmCallback alarm_cb)
        : cfg_(std::move(cfg)),
          group_(group),
          slot_(slot),
          app_(std::move(app)),
          scheduler_(scheduler),
          decode_pool_(decode_pool),
          single_cb_(std::move(single_cb)),
          tile_cb_(std::move(tile_cb)),
          want_single_(std::move(want_single)),
          want_tile_(std::move(want_tile)),
          alarm_cb_(std::move(alarm_cb)) {
        if (cfg_.task_name.empty())
            cfg_.task_name = cfg_.task_id.substr(0, std::min<size_t>(8, cfg_.task_id.size()));
        if (cfg_.channel_address.rfind("file://", 0) == 0)
            cfg_.channel_address = cfg_.channel_address.substr(7);
        file_source_ = !is_stream_source(cfg_.channel_address) &&
                       std::filesystem::exists(cfg_.channel_address);
        for (auto& m : cfg_.models) {
            const ModelSpec* spec = scheduler_.spec(m.name);
            if (!spec) {
                log_warn(cfg_.task_id + " unsupported model: " + m.name);
                continue;
            }
            auto rt = std::make_unique<ModelRuntime>();
            rt->config = m;
            rt->spec = spec;
            rt->display_label = display_label_for(*rt);
            rt->min_interval_ms = algorithm_interval_ms(rt->spec);
            max_alg_interval_ms_ = std::max(max_alg_interval_ms_, rt->min_interval_ms);
            person_grace_ms_ = std::max<int64_t>(600, max_alg_interval_ms_ * 3 / 2);
            models_.push_back(std::move(rt));
        }
        // AttrClassifier needs the production people detector; the behaviour
        // crop model uses our Damo person detector as its base.
        bool need_people = false, need_person = false;
        for (auto& rt : models_) {
            if (!rt->spec) continue;
            if (rt->spec->type == ModelType::AttrClassifier) need_people = true;
            // Production minor 195 hangs off majorId=1 (PeopleMethod), so V3
            // consumes people boxes, exactly like the 4-class helmet model.
            if (rt->spec->type == ModelType::YoloV10Crop) need_people = true;
            if (rt->spec->type == ModelType::BoatLifeJacket) need_people = true;
            // Production minorId=4 (smoking/calling) also hangs off
            // majorId=1 (PeopleMethod), so it consumes person boxes.
            if (rt->spec->type == ModelType::YoloV5Crop) need_people = true;
        }
        auto add_base = [&](const std::string& name) {
            bool has = false;
            for (auto& rt : models_)
                if (rt->spec && rt->spec->name == name) has = true;
            if (has) return;
            const ModelSpec* ps = scheduler_.spec(name);
            if (!ps) return;
            auto rt = std::make_unique<ModelRuntime>();
            rt->config.name = name;
            rt->config.conf = 0.3f;
            rt->config.nms = 0.7f;
            rt->config.exceed = 999;
            rt->spec = ps;
            rt->display_label = ps->report_content;
            rt->base_only = true;
            models_.push_back(std::move(rt));
        };
        bool people_deep = false;
        for (auto& rt : models_)
            if (rt->spec && rt->config.people_deep) people_deep = true;
        if (need_people) add_base(people_deep ? "people_big" : "people");
        // Production "人员检测基础配置": the people detector uses the
        // task-level threshold_for_people (14K: 0.85), not the old hardcoded
        // 0.3. enable_people_deep_mode is stored/logged for parity; the
        // current people model input is fixed, so it does not resize yet.
        for (auto& rt : models_) {
            if (!rt->spec ||
                (rt->spec->name != "people" && rt->spec->name != "people_big"))
                continue;
            float pc = rt->config.people_conf;
            bool deep = rt->config.people_deep;
            for (auto& o : models_) {
                if (o.get() == rt.get() || !o->spec) continue;
                pc = std::max(pc, o->config.people_conf);
                deep = deep || o->config.people_deep;
            }
            rt->config.conf = pc;
            log_info(cfg_.task_id + " people base conf=" + std::to_string(pc) +
                     " deep=" + (deep ? "1" : "0"));
        }
        if (need_person) add_base("head");
        for (auto& rt : models_) {
            for (auto& poly : rt->config.polygons) {
                if (poly.size() < 3) continue;
                bool dup = false;
                for (auto& have : roi_polygons_) {
                    if (have.size() != poly.size()) continue;
                    float d = 0.0f;
                    for (size_t i = 0; i < poly.size(); ++i) {
                        d = std::max(d, std::abs(have[i].first - poly[i].first));
                        d = std::max(d, std::abs(have[i].second - poly[i].second));
                    }
                    if (d < 0.002f) { dup = true; break; }
                }
                if (!dup) roi_polygons_.push_back(poly);
            }
        }
        if (!roi_polygons_.empty())
            log_info(cfg_.task_id + " roi polygons=" +
                     std::to_string(roi_polygons_.size()));
        for (auto& rt : models_) {
            if (!rt->spec || rt->base_only) continue;
            log_info(cfg_.task_id + " report gate model=" + rt->config.name +
                     " algo_sec=" + std::to_string(rt->config.repeat_sec) +
                     " task_sec=" + std::to_string(cfg_.cooldown_seconds) +
                     " explicit=" + (rt->config.has_repeat_param ? "1" : "0"));
        }
        if (const char* pw = std::getenv("AIDETECT_PREVIEW_WIDTH")) preview_w_ = std::max(320, std::atoi(pw));
        if (const char* ph = std::getenv("AIDETECT_PREVIEW_HEIGHT")) preview_h_ = std::max(180, std::atoi(ph));
        if (const char* pi = std::getenv("AIDETECT_PREVIEW_INTERVAL_MS"))
            preview_interval_ms_ = std::max(50, std::atoi(pi));
        // Inference demand controls.  Production never runs every algorithm on
        // every frame of every task; it skips algorithms when the pipeline is
        // behind.  These knobs give the same bounded demand so 15+ tasks can
        // share one TPU without starving a 4K, five-algorithm task.
        if (const char* dm = std::getenv("AIDETECT_DETECT_INTERVAL_MS"))
            detect_min_interval_ms_ = std::max(50, std::atoi(dm));
        if (const char* am = std::getenv("AIDETECT_ATTR_MODELS_PER_ROUND"))
            attr_models_per_round_ = std::max(1, std::atoi(am));
        if (const char* tt = std::getenv("AIDETECT_TARGET_TTL_MS"))
            target_ttl_ms_ = std::max(500, std::atoi(tt));
        if (const char* ap = std::getenv("AIDETECT_CROP_PERSONS"))
            max_crop_persons_ = std::max(1, std::atoi(ap));
        if (const char* tw = std::getenv("AIDETECT_TILE_WIDTH")) tile_w_ = std::max(160, std::atoi(tw));
        if (const char* th = std::getenv("AIDETECT_TILE_HEIGHT")) tile_h_ = std::max(90, std::atoi(th));
        if (std::getenv("AIDETECT_NO_PREVIEW")) no_preview_ = true;
        try {
            ft2_ = cv::freetype::createFreeType2();
            ft2_->loadFontData(app_.font_path, 0);
            font_ok_ = true;
            log_info("font loaded: " + app_.font_path);
        } catch (const std::exception& e) {
            log_warn("font load failed: " + app_.font_path + " " + e.what());
        }
        const char* mode = std::getenv("AIDETECT_DECODE_MODE");
        use_multi_ = mode && std::string(mode) == "multi";
        if (use_multi_) log_info(cfg_.task_id + " decode mode=multi");
        const char* label_dir_env = std::getenv("AIDETECT_LABEL_DIR");
        std::string label_dir = label_dir_env ? label_dir_env : "/app/labels";
        // Production ships label plates together with the algorithm assets; we
        // do the same: docker/labels/labels.json maps every label string to a
        // pre-rendered PNG (generated once by tools/gen_label_plates.cpp), so
        // a task only loads files instead of rendering text on startup.
        bool loaded_manifest = false;
        std::ifstream manifest(label_dir + "/labels.json");
        if (manifest.good()) {
            try {
                Json arr = Json::parse(manifest);
                for (auto& e : arr) {
                    std::string text = e.value("text", "");
                    std::string file = e.value("file", "");
                    if (text.empty() || file.empty()) continue;
                    cv::Mat m = cv::imread(label_dir + "/" + file);
                    if (!m.empty()) label_plates_[text] = m;
                }
                loaded_manifest = !label_plates_.empty();
                log_info(cfg_.task_id + " label plates from manifest=" +
                         std::to_string(label_plates_.size()));
            } catch (const std::exception& e) {
                log_warn(cfg_.task_id + " labels.json: " + std::string(e.what()));
            }
        }
        if (!loaded_manifest) {
            // Legacy layout: six hard-coded PNG names.
            std::vector<std::string> names = {"未戴安全帽", "未穿反光衣", "未穿救生衣",
                                              "未穿安全帽", "抽烟", "人员"};
            for (size_t i = 0; i < names.size(); ++i) {
                cv::Mat m = cv::imread(label_dir + "/label_" + std::to_string(i) + ".png");
                if (!m.empty()) label_plates_[names[i]] = m;
            }
        }
        // Safety net for a label that is not in the manifest yet (e.g. a new
        // algorithm): render it once and warn so the manifest gets regenerated.
        for (auto& rt : models_) {
            if (!rt->spec) continue;
            const std::string text = rt->display_label.empty()
                                         ? rt->spec->report_content
                                         : rt->display_label;
            if (!text.empty() && !label_plates_.count(text)) {
                log_warn(cfg_.task_id + " label not pre-rendered: " + text);
                ensure_label_plate(text);
            }
        }
        log_info(cfg_.task_id + " label plates=" + std::to_string(label_plates_.size()));
    }
    // Render a red plate with white Chinese text for a label that has no PNG.
    // Same look as the production-style plates: red background, white glyphs,
    // bottom edge sits on the box top.  Cached in label_plates_ for the fast
    // preview path (no per-frame CPU text drawing).
    void ensure_label_plate(const std::string& text) {
        if (text.empty() || label_plates_.count(text)) return;
        if (!font_ok_ || !ft2_) return;
        try {
            // Draw on a generous canvas and crop to the pixels that actually
            // became non-red.  getTextSize under-reports the rendered advance
            // for some glyphs (the last character of 抽烟/打电话 was clipped
            // even with a 22px right pad), so measure the drawn result instead
            // of trusting the metric.
            const int font = 30;
            cv::Mat canvas(96, 640, CV_8UC3, cv::Scalar(0, 0, 255));
            ft2_->putText(canvas, text, cv::Point(12, 8), font,
                          cv::Scalar(255, 255, 255), 1, cv::LINE_AA, false);
            cv::Mat mask;
            cv::inRange(canvas, cv::Scalar(200, 200, 200),
                        cv::Scalar(255, 255, 255), mask);
            cv::Rect bb = cv::boundingRect(mask);
            if (bb.width <= 0 || bb.height <= 0) return;
            const int pad_x = 8, pad_y = 4;
            int x0 = std::max(0, bb.x - pad_x);
            int y0 = std::max(0, bb.y - pad_y);
            int x1 = std::min(canvas.cols, bb.x + bb.width + pad_x);
            int y1 = std::min(canvas.rows, bb.y + bb.height + pad_y);
            cv::Mat m = canvas(cv::Rect(x0, y0, x1 - x0, y1 - y0)).clone();
            label_plates_[text] = m;
            log_info(cfg_.task_id + " label plate rendered: " + text + " " +
                     std::to_string(m.cols) + "x" + std::to_string(m.rows));
        } catch (const std::exception& e) {
            log_warn(cfg_.task_id + " label plate: " + std::string(e.what()));
        }
    }
    ~Task() {
        stop();
        join();
    }
    const TaskConfig& config() const { return cfg_; }
    int group() const { return group_; }
    int slot() const { return slot_; }
    bool everFrame() const { return ever_frame_.load(); }
    void set_stream_error(const std::string& s) {
        std::lock_guard<std::mutex> lk(stream_err_mu_);
        last_stream_error_ = s.substr(0, 240);
    }
    std::string stream_error() const {
        std::lock_guard<std::mutex> lk(stream_err_mu_);
        return last_stream_error_;
    }
    std::string short_stream_error() const {
        std::string e = stream_error();
        if (e.find("missing data.url") != std::string::npos ||
            e.find("Camera not found") != std::string::npos ||
            e.find("cameraIndexCode") != std::string::npos)
            return "Camera not found";
        if (e.find("Input/output") != std::string::npos ||
            e.find("I/O error") != std::string::npos ||
            e.find("DESCRIBE failed") != std::string::npos ||
            e.find("5XX") != std::string::npos ||
            e.find("gateway exited") != std::string::npos ||
            e.find("upstream no video") != std::string::npos)
            return "Input/output error";
        return "";
    }
    // Bumped whenever the run loop makes progress (candidate attempt, gateway
    // start, frame).  A task that stops bumping this is wedged inside a
    // blocking call and must be rebuilt by the supervisor.
    int64_t lastProgressAgeMs() const {
        const int64_t p = last_progress_ms_.load();
        return p > 0 ? now_ms() - p : 0;
    }
    int64_t lastFrameAgeMs() const {
        int64_t base = ever_frame_.load() ? last_frame_ms_.load() : started_ms_.load();
        if (base <= 0) return 0;
        return std::max<int64_t>(0, now_ms() - base);
    }
    bool running() const { return running_; }
    bool failed() const { return failed_; }
    std::string error() const { return error_; }
    void start() {
        if (running_) return;
        if (models_.empty()) {
            failed_ = true;
            error_ = "no supported model in task config";
            return;
        }
        running_ = true;
        infer_thread_ = std::thread([this] { infer_loop(); });
        preview_thread_ = std::thread([this] { preview_loop(); });
        thread_ = std::thread([this] { run(); });
        status_thread_ = std::thread([this] {
            int last_heartbeat_s = -1;
            while (!stop_) {
            // Auto substream: while nobody watches this single channel, ISC
            // tasks decode the substream (streamType=1) instead of the 4K main
            // stream.  Opening the channel in the Web UI switches back to main.
            // Enabled by /tmp/aidetect_auto_substream (runtime, no restart).
            if (cfg_.channel_address.rfind("isc://", 0) == 0 &&
                std::ifstream("/tmp/aidetect_auto_substream").good()) {
                const bool watching = want_single_ && want_single_();
                const int want = watching ? 0 : 1;
                if (want != isc_stream_pref_.load()) {
                    isc_stream_pref_ = want;
                    log_info(cfg_.task_id + " switch ISC streamType=" +
                             std::to_string(want) +
                             (watching ? " (viewer)" : " (idle, save cpu)"));
                    stop_gateway();
                }
            }
            if (cfg_.channel_address.rfind("rtsp://", 0) == 0 &&
                std::ifstream("/tmp/aidetect_auto_substream").good()) {
                const std::string sub = substream_url(cfg_.channel_address);
                if (!sub.empty()) {
                    const bool watching = want_single_ && want_single_();
                    const int want = watching ? 0 : 1;
                    if (want != direct_stream_pref_.load()) {
                        direct_stream_pref_ = want;
                        reconnect_requested_ = true;
                        log_info(cfg_.task_id + " switch camera stream=" +
                                 (watching ? std::string("main") : std::string("sub")));
                    }
                }
            }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                int64_t now = now_ms();
                // A stalled/never-connected channel must keep producing a
                // *fresh* status image, otherwise the browser shows one frozen
                // clock forever and cannot tell "reconnecting" from "dead".
                const int64_t last = ever_frame_.load() ? last_frame_ms_.load()
                                                        : started_ms_.load();
                const bool stale = last > 0 && now - last > 3000;
                if (stale) need_recover_status_ = true;
                if (stale && now / 1000 != last_heartbeat_s) {
                    last_heartbeat_s = static_cast<int>(now / 1000);
                    std::string text;
                    {
                        std::lock_guard<std::mutex> lk(status_text_mu_);
                        text = stream_status_;
                    }
                    std::string serr = short_stream_error();
                    if (!serr.empty())
                        text = serr;
                    if (text.empty())
                        text = ever_frame_.load() ? "视频流卡住，正在重连..."
                                                  : "正在连接监控视频流...";
                    int64_t down_s = std::max<int64_t>(0, (now - last) / 1000);
                    if (serr.empty() && down_s > 0)
                        text += " 已断开 " + std::to_string(down_s) + "s";
                    // Do not store the rendered "已断开 Ns" suffix as the next
                    // base status, otherwise it repeats every heartbeat.
                    publish_status(text, true, false);
                }
                if (!ever_frame_.load() && now - started_ms_ > 2500) {
                    std::string serr = short_stream_error();
                    set_stream_status(serr.empty() ? "正在连接监控视频流..." : serr);
                }
                // A sail Decoder::read() can block forever when the ISC/RTMP
                // stream stalls (the UI then shows a frozen last frame and
                // "decoder opened" is the only log). Kill the gateway to make
                // read() return so the existing reconnect path runs.
                // Do not kill the gateway after only 15s: the packet ring
                // still holds the pre-alarm clip, and killing it loses the
                // only source of the alarm video. 45s still recovers a truly
                // dead stream without destroying an imminent clip.
                bool stalled = ever_frame_.load() && (now - last > 45000);
                bool never = (!ever_frame_.load() && now - started_ms_ > 60000);
                if ((stalled || never) && !gateway_pid_.empty() &&
                    now - last_stall_restart_ms_ > 60000) {
                    last_stall_restart_ms_ = now;
                    log_warn(cfg_.task_id + (ever_frame_.load()
                                                 ? " frame watchdog: no frame for 45s, restart gateway"
                                                 : " frame watchdog: no first frame for 30s, restart gateway"));
                    publish_status("视频流卡住，正在重连...");
                    stop_gateway();
                    // Decoder::read() blocks on an empty UDP socket with no
                    // timeout. Closing the gateway alone does not wake it;
                    // send a few garbage TS packets to force recv() back so
                    // the existing fail/reconnect path can run.
                    wake_decoder_udp();
                }
            }
        });
    }
    void release_high_res() {
        if (high_res_held_.exchange(false)) high_res_gate().release();
    }
    struct HighResGuard {
        Task* task = nullptr;
        bool held = false;
        ~HighResGuard() { if (held) task->release_high_res(); }
    };
    void stop() {
        stop_ = true;
        // Kill the local gateway first: a blocked decoder read on the UDP
        // input can otherwise keep thread_.join() waiting forever.
        stop_gateway();
        // A blocked read never reaches the local guard destructor.  Release
        // the high-res slot here so an automatic/manual restart can acquire
        // it again.
        release_high_res();
    }
    void debug_trigger() { debug_alarm_ = true; }
    // Hot config update: the infer thread rebuilds models/ROI between frames,
    // matching production's TaskChannel::archiveUpdated -> taskArchiveApply
    // path (no task restart).  A source/name change still needs a restart.
    bool can_hot_update(const TaskConfig& tc) const {
        return tc.channel_address == cfg_.channel_address &&
               tc.task_name == cfg_.task_name;
    }
    void update_config(const TaskConfig& tc) {
        std::lock_guard<std::mutex> lk(pending_cfg_mu_);
        pending_cfg_ = tc;
        has_pending_cfg_ = true;
    }
    void join() {
        stop_gateway();
        if (thread_.joinable()) thread_.join();
        stop_recorder();
        stop_gateway();
        preview_cv_.notify_all();
        if (preview_thread_.joinable()) preview_thread_.join();
        infer_cv_.notify_all();
        if (infer_thread_.joinable()) infer_thread_.join();
        if (status_thread_.joinable()) status_thread_.join();
        running_ = false;
    }
    std::shared_ptr<std::vector<unsigned char>> latest_jpeg() {
        std::lock_guard<std::mutex> lk(jpeg_mutex_);
        return latest_jpeg_;
    }
    // Configuration page must show the raw camera frame: no boxes, labels or
    // task OSD.  Production uses the clean frame as the ROI background.
    std::shared_ptr<std::vector<unsigned char>> area_jpeg() {
        std::lock_guard<std::mutex> lk(area_jpeg_mutex_);
        return latest_area_jpeg_;
    }
    // /task/get_area_frame sets this so we only pay for a full-size area render
    // while the configuration page is actually open (2s TTL) or right after a
    // request (force_area_ makes the next decoded frame render immediately).
    void mark_area_request() {
        area_request_ms_ = now_ms();
        force_area_ = true;
    }
    Json status() const {
        Json j;
        j["taskId"] = cfg_.task_id;
        j["state"] = failed_ ? "error" : (running_ ? "running" : "stopped");
        j["group"] = group_;
        j["slot"] = slot_;
        j["error"] = error_;
        j["taskName"] = cfg_.task_name;
        j["channelAddress"] = cfg_.channel_address;
        if (frame_w_ > 0 && frame_h_ > 0)
            j["resolution"] = std::to_string(frame_w_) + "x" + std::to_string(frame_h_);
        else
            j["resolution"] = "";
        std::string health = "stopped";
        if (running_) {
            if (failed_ || !error_.empty()) health = "error";
            else if (!ever_frame_.load()) health = "connecting";
            else if (lastFrameAgeMs() > 5000) health = "stalled";
            else health = "ok";
        }
        j["health"] = health;
        std::string last_error = error_;
        if (last_error.empty()) last_error = stream_error();
        j["lastError"] = last_error;
        Json ms = Json::array();
        {
            std::lock_guard<std::mutex> lk(models_mu_);
            for (auto& rt : models_) ms.push_back(rt->spec->name);
        }
        j["models"] = ms;
        j["lastFrameAgeMs"] = lastFrameAgeMs();
        j["lastProgressAgeMs"] = lastProgressAgeMs();
        j["everFrame"] = ever_frame_.load();
        return j;
    }
private:
    struct ModelRuntime {
        ModelConfig config;
        const ModelSpec* spec = nullptr;
        std::deque<bool> window;
        bool base_only = false;  // internal person detector feeding classifiers
        // Label drawn on preview/alarm images.  Behaviour models compose it
        // from the enabled behaviours (抽烟/打电话/玩手机 joined with "/").
        std::string display_label;
        // Per-algorithm cadence: production runs each algorithm as its own
        // pipeline and skips frames it cannot keep up with.  A crop
        // algorithm only needs to refresh a few times per second; running
        // all of them on every round is what multiplied the VPP work.
        int64_t min_interval_ms = 0;
        int64_t last_run_ms = 0;
        bool confirmed = false;  // blue countdown passed -> red until target gone
        int64_t first_seen_ms = 0;
        int64_t last_seen_ms = 0;
        int64_t last_alarm_ms = 0;
        // Next allowed upload for this algorithm.  Kept separately from the
        // per-target `alarmed` flag so a 86400s minimum interval survives
        // target churn and preview/inference hiccups.
        int64_t next_report_ms = 0;
        int64_t last_log_ms = 0;
        struct TargetState {
            bool confirmed = false;
            bool alarmed = false;
            int64_t first_seen_ms = 0;
            int64_t last_seen_ms = 0;
            int64_t last_alarm_ms = 0;
            cv::Rect2f rect;
        };
        std::vector<TargetState> targets;  // independent per-target countdown state
    };
    // Compose the visible label from the *enabled* behaviours so a task that
    // only enables smoking shows 抽烟, not 抽烟/打电话.  Multiple enabled
    // behaviours are joined with "/" into one label (production style).
    // Per-algorithm refresh interval.  Production pipelines each algorithm
    // and drops frames when behind (onAllAlgoSkipped); we bound how often
    // a crop algorithm runs instead of running all of them every round.
    static int64_t algorithm_interval_ms(const ModelSpec* spec) {
        if (!spec) return 0;
        int64_t base = 400;
        if (const char* e = std::getenv("AIDETECT_ALG_INTERVAL_MS"))
            base = std::max<int64_t>(0, std::atoi(e));
        if (base <= 0) return 0;
        switch (spec->type) {
            case ModelType::AttrClassifier:
            case ModelType::YoloV10Crop:
                return base;
            case ModelType::BoatLifeJacket:
                return base * 5 / 4;
            case ModelType::YoloV5Crop:
                return base * 3 / 2;
            default:
                return 0;  // base person detector runs every round
        }
    }
    static std::string display_label_for(const ModelRuntime& rt) {
        const bool behaviour =
            rt.spec && (rt.spec->type == ModelType::YoloV5Crop ||
                         rt.spec->type == ModelType::YoloV5 ||
                         rt.spec->name == "behavior" ||
                         rt.spec->name == "smoking_call");
        if (behaviour) {
            std::vector<std::string> parts;
            if (rt.config.behavior_enable_smoking) parts.push_back("抽烟");
            if (rt.config.behavior_enable_calling) parts.push_back("打电话");
            if (rt.config.behavior_enable_playing) parts.push_back("玩手机");
            if (!parts.empty()) {
                std::string s;
                for (size_t i = 0; i < parts.size(); ++i) {
                    if (i) s += "/";
                    s += parts[i];
                }
                return s;
            }
        }
        return rt.spec ? rt.spec->report_content : std::string();
    }
    struct PendingVideo {
        bool active = false;
        std::string tmp_path;
        std::string final_path;
        Json report;
        int64_t ready_ms = 0;
    };
    void run() {
        started_ms_ = now_ms();
        last_frame_ms_ = 0;
        ever_frame_ = false;
        while (!stop_) {
            failed_ = false;
            error_.clear();
            running_ = true;
            publish_status("正在连接监控视频流...");
            try {
                if (use_multi_) {
                    run_multi();
                } else {
                    run_single();
                }
            } catch (const std::exception& e) {
                log_warn(cfg_.task_id + " stream loop: " + e.what());
            }
            if (stop_) break;
            publish_status("视频流连接中断，正在重连...");
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    }
    // Derive the substream URL for the common IP-camera layouts so idle
    // channels can decode a ~10x smaller stream.
    static std::string substream_url(const std::string& s) {
        std::string u = s;
        auto repl = [&](const std::string& from, const std::string& to) {
            auto p = u.find(from);
            if (p == std::string::npos) return false;
            u.replace(p, from.size(), to);
            return true;
        };
        if (repl("/h264/ch1/main/av_stream", "/h264/ch1/sub/av_stream")) return u;
        if (repl("/Streaming/Channels/101", "/Streaming/Channels/102")) return u;
        if (repl("subtype=0", "subtype=1")) return u;
        if (repl("streamType=0", "streamType=1")) return u;
        return {};
    }
    // Reconnect request used by direct-decode tasks when the stream preference
    // changes (main <-> sub).
    std::atomic<bool> reconnect_requested_{false};
    std::atomic<int> direct_stream_pref_{-1};
    static bool is_stream_source(const std::string& s) {
        return s.rfind("rtsp://", 0) == 0 || s.rfind("udp://", 0) == 0 ||
               s.rfind("rtmp://", 0) == 0 || s.rfind("http://", 0) == 0 ||
               s.rfind("https://", 0) == 0;
    }
    // 1.5s, 3s, 6s, 12s, 24s, 30s for the first failures, then a 2-minute
    // probe: a permanently dead ISC channel must not keep re-initialising the
    // VPU and stealing decode time from healthy channels. Recovery is still
    // automatic within two minutes.
    int open_backoff_ms() const {
        if (open_fail_count_ >= 6) return 120000;
        int n = std::min(open_fail_count_, 4);
        return std::min(30000, 1500 * (1 << n));
    }
    void run_multi() {
        try {
            sail::Handle handle(0);
            sail::Bmcv bmcv(handle);
            std::string source = cfg_.channel_address;
            if (source.rfind("isc://", 0) == 0) {
                int isc_retry = 0;
                while (!stop_) {
                    try {
                        source = IscResolver::resolve(source);
                        isc_retry = 0;
                        log_info(cfg_.task_id + " ISC resolved to rtsp");
                        break;
                    } catch (const std::exception& e) {
                        log_warn(cfg_.task_id + " ISC retry: " + e.what());
                        publish_status("ISC 地址解析失败，正在重试...");
                        int wait = std::min(30000, 2000 << std::min(4, isc_retry));
                        wait += (isc_retry % 5) * 300;
                        ++isc_retry;
                        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
                    }
                }
                if (stop_) {
                    running_ = false;
                    return;
                }
            }
            if (!is_stream_source(source) && !std::filesystem::exists(source)) {
                failed_ = true;
                error_ = "unsupported channel address: " + source;
                log_error(cfg_.task_id + " " + error_);
                running_ = false;
                return;
            }
            source_url_ = source;
            start_recorder();
            int ch = -1;
            while (!stop_) {
                ch = decode_pool_.add(group_, slot_, source);
                if (ch >= 0) {
                    open_fail_count_ = 0;
                    break;
                }
                ++open_fail_count_;
                publish_status("正在连接监控视频流...");
                std::this_thread::sleep_for(std::chrono::milliseconds(open_backoff_ms()));
            }
            if (stop_ || ch < 0) {
                running_ = false;
                return;
            }
            std::vector<int> shape;
            for (int i = 0; i < 100; ++i) {
                shape = decode_pool_.shape(group_, slot_);
                if (shape.size() >= 4 && shape[2] > 0 && shape[3] > 0) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            while (!stop_ &&
                   !(shape.size() >= 4 && shape[2] > 0 && shape[3] > 0)) {
                publish_status("视频流未就绪，正在重试...");
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                shape = decode_pool_.shape(group_, slot_);
            }
            if (stop_) {
                running_ = false;
                return;
            }
            frame_w_ = shape[3];
            frame_h_ = shape[2];
            log_info(cfg_.task_id + " multidecoder opened shape=" +
                     ModelInstance::shape_to_string(shape));
            HighResGuard high_res_lease{this};
            if (frame_h_ >= 1000) {
                if (!high_res_gate().acquire()) {
                    failed_ = true;
                    error_ = "high-resolution task capacity reached (max " +
                             std::to_string(high_res_gate().max()) + ")";
                    log_error(cfg_.task_id + " " + error_);
                    running_ = false;
                    return;
                }
                high_res_lease.held = true;
                high_res_held_ = true;
            }
            const char* grace_env = std::getenv("AIDETECT_TASK_START_GRACE_MS");
            int grace_ms = grace_env ? std::max(0, std::atoi(grace_env)) : 0;
            if (grace_ms > 0 && !stop_)
                std::this_thread::sleep_for(std::chrono::milliseconds(grace_ms));
            if (!no_dump_env()) {
                // Must be enabled before the first read(); enabling after a
                // frame has already been decoded can crash in SAIL dump.
                decode_pool_.enable_dump(
                    group_, slot_, std::max(12, cfg_.video_duration + 2));
            }
                        int frame_id = 0;
            sail::BMImage frame;  // reused: one allocation per task, not per frame
            while (!stop_) {
                int ret = decode_pool_.read(group_, slot_, frame);
                finalize_video(false);
                if (ret != 0) {
                    publish_status("正在重连监控视频流...");
                    // MultiDecoder reconnects internally in a tight loop that
                    // floods the log and burns a core (observed: channel 3
                    // spinning at 5003).  Remove the channel instead, back off,
                    // then re-add with a freshly resolved URL on the next loop.
                    if (!stop_) {
                        decode_pool_.remove(group_, slot_);
                        ++open_fail_count_;
                        std::this_thread::sleep_for(std::chrono::milliseconds(
                            open_backoff_ms()));
                        if (cfg_.channel_address.rfind("isc://", 0) == 0) {
                            try {
                                source = IscResolver::resolve(cfg_.channel_address);
                                source_url_ = source;
                            } catch (const std::exception& e) {
                                log_warn(cfg_.task_id + " ISC re-resolve: " + e.what());
                            }
                        }
                        int re = decode_pool_.add(group_, slot_, source);
                        if (re >= 0) {
                            open_fail_count_ = 0;
                            publish_status("监控视频流已恢复");
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                process_frame(frame, handle, bmcv, frame_id);
            }
            if (pending_.active) {
                int64_t wait_ms = pending_.ready_ms + 1500 - now_ms();
                if (wait_ms > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
            }
            finalize_video(true);
            log_info(cfg_.task_id + " multidecoder task stopped");
        } catch (const std::exception& e) {
            failed_ = true;
            error_ = e.what();
            log_error(cfg_.task_id + " multidecoder exception: " + error_);
        }
    }
    void run_single() {
        std::shared_ptr<sail::Decoder> decoder_holder;
        try {
            sail::Handle handle(0);
            sail::Bmcv bmcv(handle);
            std::string source = cfg_.channel_address;
            // Idle cameras decode their substream to save CPU; opening the
            // channel in the Web UI switches back to the main stream.
            if (direct_stream_pref_.load() == 1) {
                const std::string sub = substream_url(source);
                if (!sub.empty()) source = sub;
            }
            const bool isc_source = source.rfind("isc://", 0) == 0;
            if (!isc_source && !is_stream_source(source) &&
                !std::filesystem::exists(source)) {
                failed_ = true;
                error_ = "unsupported channel address: " + source;
                log_error(cfg_.task_id + " " + error_);
                running_ = false;
                return;
            }
            upstream_url_ = source;
            while (!stop_) {
                if (gateway_pid_.empty()) {
                    bool gateway_ok = true;
                    // ISC URLs are single-use: resolve() is called on every attempt and
                    // isc_rot_ rotates the candidate when an open or read fails.
                    // ISC keeps the gateway: 36 direct sail::Decoder objects exhaust the
                    // VPU on this box (verified: whole box stalls, reboot
                    // needed).  Non-ISC sources decode in-process.
                    const bool local_file = !is_stream_source(cfg_.channel_address);
                    // Files have no live pacing: sail::Decoder on a file
                    // returns EOF after one pass, the task then reconnects
                    // every few seconds (that reconnect storm was the RSS
                    // leak driver).  Files and ISC go through the ingest
                    // bridge, which paces and loops them with monotonic TS.
                    if (direct_decode_enabled() && !isc_source && !local_file) {
                        source_url_ = direct_decode_url();
                        gateway_ok = !source_url_.empty();
                    } else {
                        gateway_ok = isc_source ? start_isc_gateway()
                                               : start_gateway(source);
                    }
                    if (!gateway_ok) {
                        ++open_fail_count_;
                        publish_status("正在连接监控视频流...");
                        std::this_thread::sleep_for(std::chrono::milliseconds(open_backoff_ms()));
                        continue;
                    }
                    start_recorder();
                }
                try {
                    std::lock_guard<std::mutex> hw(decode_hw_mutex());
                    decoder_holder = std::make_shared<sail::Decoder>(source_url_, true, 0);
                    decoder_ = decoder_holder;
                    if (decoder_->is_opened()) {
                        open_fail_count_ = 0;
                        break;
                    }
                } catch (const std::exception& e) {
                    log_warn(cfg_.task_id + " decoder open: " + e.what());
                    if (cfg_.channel_address.rfind("isc://", 0) == 0) {
                        // Codec open failed although the gateway has data:
                        // force the next ISC candidate (other streamType or
                        // protocol) instead of retrying the same stream.
                        ++isc_rot_;
                        stop_gateway();
                        ++open_fail_count_;
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(open_backoff_ms()));
                    }
                    set_stream_error(std::string("decoder open: ") + e.what());
                    decoder_holder.reset();
                    decoder_.reset();
                }
                ++open_fail_count_;
                publish_status("正在连接监控视频流...");
                // Persistent failures must not hammer the VPU: 24 unreachable
                // ISC channels retrying every 1.5s starved the live decoders.
                std::this_thread::sleep_for(std::chrono::milliseconds(open_backoff_ms()));
            }
            if (stop_ || !decoder_ || !decoder_->is_opened()) {
                running_ = false;
                return;
            }
            auto frame_shape = decoder_->get_frame_shape();
            while (!stop_ &&
                   !(frame_shape.size() >= 4 && frame_shape[2] > 0 &&
                     frame_shape[3] > 0)) {
                publish_status("视频流未就绪，正在重试...");
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                if (decoder_) frame_shape = decoder_->get_frame_shape();
            }
            if (stop_) {
                running_ = false;
                return;
            }
            frame_w_ = frame_shape[3];
            frame_h_ = frame_shape[2];
            log_info(cfg_.task_id + " decoder opened shape=" +
                     ModelInstance::shape_to_string(frame_shape));
            HighResGuard high_res_lease{this};
            if (frame_h_ >= 1000) {
                if (!high_res_gate().acquire()) {
                    failed_ = true;
                    error_ = "high-resolution task capacity reached (max " +
                             std::to_string(high_res_gate().max()) + ")";
                    log_error(cfg_.task_id + " " + error_);
                    running_ = false;
                    return;
                }
                high_res_lease.held = true;
                high_res_held_ = true;
                log_info(cfg_.task_id + " high-res slot=" +
                         std::to_string(high_res_gate().active()) + "/" +
                         std::to_string(high_res_gate().max()));
            }
            if (!no_dump_env())
                decoder_->enable_dump(std::max(12, cfg_.video_duration + 2));
                        int frame_id = 0;
            int fail_count = 0;
            sail::BMImage frame;  // reused: one allocation per task, not per frame
            while (!stop_) {
                // Snapshot the shared_ptr: a concurrent stop/reconnect may reset
                // the member, and read() must never run on a freed Decoder.
                std::shared_ptr<sail::Decoder> dec = decoder_;
                int ret = dec ? dec->read(handle, frame) : -1;
                finalize_video(false);
                if (ret != 0) {
                    ++fail_count;
                    publish_status("正在重连监控视频流...");
                    if (fail_count >= 5) {
                        decoder_holder.reset();
                        decoder_.reset();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
                        bool gateway_ok = true;
                        // ISC URLs are single-use: resolve() is called on every attempt and
                        // isc_rot_ rotates the candidate when an open or read fails.
                        // ISC keeps the gateway: 36 direct sail::Decoder objects exhaust the
                    // VPU on this box (verified: whole box stalls, reboot
                    // needed).  Non-ISC sources decode in-process.
                    const bool local_file = !is_stream_source(cfg_.channel_address);
                    // Files have no live pacing: sail::Decoder on a file
                    // returns EOF after one pass, the task then reconnects
                    // every few seconds (that reconnect storm was the RSS
                    // leak driver).  Files and ISC go through the ingest
                    // bridge, which paces and loops them with monotonic TS.
                    if (direct_decode_enabled() && !isc_source && !local_file) {
                            source_url_ = direct_decode_url();
                            gateway_ok = !source_url_.empty();
                        } else {
                            gateway_ok = isc_source ? start_isc_gateway()
                                                   : start_gateway(source);
                        }
                        if (!gateway_ok) {
                            ++open_fail_count_;
                            std::this_thread::sleep_for(std::chrono::milliseconds(open_backoff_ms()));
                            continue;
                        }
                        restart_recorder();
                        try {
                            std::lock_guard<std::mutex> hw(decode_hw_mutex());
                            decoder_holder = std::make_shared<sail::Decoder>(source_url_, true, 0);
                            if (decoder_holder->is_opened()) {
                                decoder_ = decoder_holder;
                                if (!no_dump_env())
                                    decoder_->enable_dump(
                                        std::max(12, cfg_.video_duration + 2));
                                fail_count = 0;
                                log_info(cfg_.task_id + " decoder reconnected");
                                publish_status("监控视频流已恢复");
                            } else {
                                decoder_.reset();
                                if (isc_source) ++isc_rot_;
                                publish_status("重连失败，继续重试");
                            }
                        } catch (const std::exception& e) {
                            decoder_.reset();
                            if (isc_source) ++isc_rot_;
                            publish_status(std::string("重连失败: ") + e.what());
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
                fail_count = 0;
                process_frame(frame, handle, bmcv, frame_id);
                if (reconnect_requested_.exchange(false)) {
                    // Stream preference changed: drop the decoder so the outer
                    // loop reopens with the new URL (main <-> sub).
                    decoder_holder.reset();
                    decoder_.reset();
                    break;
                }
            }
            if (pending_.active) {
                int64_t wait_ms = pending_.ready_ms + 1500 - now_ms();
                if (wait_ms > 0) {
                    log_info(cfg_.task_id + " waiting for video dump before stop");
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
                }
            }
            finalize_video(true);
            decoder_holder.reset();
            decoder_.reset();
            log_info(cfg_.task_id + " task stopped");
        } catch (const std::exception& e) {
            failed_ = true;
            error_ = e.what();
            decoder_holder.reset();
            decoder_.reset();
            log_error(cfg_.task_id + " task exception: " + error_);
        }
        running_ = false;
    }
    // Production keeps per-channel pooled frames (dmabuf stayed at 133 over
    // 39 h) while allocating one image per resize grew ours to 1359 in 15 h.
    // Cache the VPP/model input images per (format,size) and reuse them; the
    // inference thread is the only user, so no locking is needed here.
    std::map<uint64_t, sail::BMImage> infer_pool_;
    sail::BMImage* pooled_frame(sail::Handle& handle, bm_image_format_ext fmt,
                                bm_image_data_format_ext dtype, int w, int h) {
        const uint64_t key = (static_cast<uint64_t>(fmt) << 48) |
                             (static_cast<uint64_t>(w) << 20) |
                             static_cast<uint64_t>(h);
        auto it = infer_pool_.find(key);
        if (it != infer_pool_.end()) return &it->second;
        try {
            auto res = infer_pool_.emplace(
                key, sail::BMImage(handle, h, w, fmt, dtype));
            if (res.second) return &res.first->second;
        } catch (const std::exception& e) {
            log_warn(cfg_.task_id + " pool alloc: " + e.what());
        }
        return nullptr;
    }

    void detect(sail::BMImage& frame, sail::Handle& handle, sail::Bmcv& bmcv,
                int64_t frame_ts) {
        const int64_t prof_t0 = now_ms();
        current_frame_ms_ = frame_ts;
        int w = frame.width();
        int h = frame.height();
        if (w <= 0 || h <= 0) return;
        if (frame_w_ <= 0) {
            frame_w_ = w;
            frame_h_ = h;
        }
        std::vector<Detection> all;
        std::vector<Detection> base;
        // Full-frame person boxes from the base detector.  These are not
        // drawn, but they are the stable anchor that tells the preview
        // tracker whether an attribute box (vest/boat/helmet) is still on a
        // real person or has become a ghost after the person left.
        std::vector<Detection> persons;
        // Production gives every algorithm its own pipeline and simply skips
        // algorithms on frames it cannot keep up with
        // (`TaskInstance::onAllAlgoSkipped`).  Running all four crop
        // algorithms over every person on every frame is what made a 4K task
        // need ~33 TPU passes per round and starve under 15 tasks.  Rotate a
        // bounded number of crop algorithms per round instead: each algorithm
        // still refreshes at a few Hz, the total demand stays flat.
        std::vector<std::string> crop_names;
        for (auto& rt : models_) {
            if (!rt->spec) continue;
            auto t = rt->spec->type;
            if (t == ModelType::YoloV10Crop || t == ModelType::YoloV5Crop ||
                t == ModelType::BoatLifeJacket || t == ModelType::AttrClassifier)
                crop_names.push_back(rt->spec->name);
        }
        std::vector<std::string> enabled;
        if (!crop_names.empty()) {
            size_t k = std::min(attr_models_per_round_, crop_names.size());
            for (size_t i = 0; i < k; ++i)
                enabled.push_back(crop_names[(attr_turn_ + i) % crop_names.size()]);
            ++attr_turn_;
        }
        auto crop_enabled = [&](const std::string& name) {
            for (auto& e : enabled)
                if (e == name) return true;
            return false;
        };
        for (auto& rt : models_) {
            if (rt->spec->type == ModelType::AttrClassifier) continue;
            // Crop models (helmet V3, behaviour) run only on person crops
            // below; a full-frame inference here wastes a TPU pass and its
            // 256x256 output is meaningless for a 4K frame.
            if (rt->spec->type == ModelType::YoloV10Crop ||
                rt->spec->type == ModelType::YoloV5Crop ||
                rt->spec->type == ModelType::BoatLifeJacket)
                continue;
            int net_w = rt->spec->input_w > 0 ? rt->spec->input_w : 640;
            int net_h = rt->spec->input_h > 0 ? rt->spec->input_h : 640;
            float r = std::min(static_cast<float>(net_w) / w,
                               static_cast<float>(net_h) / h);
            int tw = static_cast<int>(std::round(w * r));
            int th = static_cast<int>(std::round(h * r));
            int tx = (net_w - tw) / 2;
            int ty = (net_h - th) / 2;
            sail::PaddingAtrr pad;
            pad.set_stx(tx);
            pad.set_sty(ty);
            pad.set_w(tw);
            pad.set_h(th);
            pad.set_r(114);
            pad.set_g(114);
            pad.set_b(114);
            sail::BMImage* resized = pooled_frame(
                handle, frame.format(), DATA_TYPE_EXT_1N_BYTE, net_w, net_h);
            sail::BMImage* pre = pooled_frame(
                handle, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, net_w, net_h);
            if (!resized || !pre) continue;
            if (bmcv.vpp_crop_and_resize_padding(frame, *resized, 0, 0, w, h,
                                                 net_w, net_h, pad,
                                                 bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0)
                continue;
            bmcv.convert_to(*resized, *pre,
                            std::make_tuple(
                                std::make_pair(rt->spec->alpha, rt->spec->beta),
                                std::make_pair(rt->spec->alpha, rt->spec->beta),
                                std::make_pair(rt->spec->alpha, rt->spec->beta)));
            InferOutput out = scheduler_.infer(*rt->spec, *pre, handle, bmcv);
            if (rt->spec->type == ModelType::YoloV10Crop) continue;
            std::vector<Detection> dets;
            if (rt->spec->type == ModelType::Damo) {
                dets = postprocess_damo(out, *rt, w, h, r, tx, ty);
            } else if (rt->spec->type == ModelType::YoloV5) {
                dets = postprocess_yolov5(out, *rt, w, h, r, tx, ty);
            } else if (rt->spec->type == ModelType::YoloV8) {
                dets = postprocess_yolov8(out, *rt, w, h, r, tx, ty);
            } else if (rt->spec->type == ModelType::YoloV10) {
                dets = postprocess_yolov10(out, *rt, w, h, r, tx, ty);
            } else if (rt->spec->type == ModelType::CrowdHead) {
                dets = postprocess_crowd(out, *rt, w, h, r, tx, ty);
            } else {
                dets = postprocess_yolo(out, *rt, w, h, r, tx, ty);
            }
            dets = filter_roi(dets, *rt, w, h);
            if (rt->base_only && !dets.empty()) {
                // The 4K construction feeds occasionally produce a very wide
                // false person box on a wall/excavator (aspect > 2).  Such a
                // box then gets a no-vest/no-lifejacket label and is carried
                // by the preview tracker as a large empty rectangle.  Reject
                // implausible person geometry before it reaches any attribute
                // classifier.
                std::vector<Detection> plausible;
                plausible.reserve(dets.size());
                for (auto& d : dets) {
                    float bw = d.x2 - d.x1, bh = d.y2 - d.y1;
                    if (bw < 8 || bh < 16) continue;
                    float aspect = bw / std::max(1.0f, bh);
                    if (aspect < 0.12f || aspect > 1.6f) continue;
                    if (bh > h * 0.92f || bw > w * 0.80f) continue;
                    plausible.push_back(d);
                }
                dets.swap(plausible);
            }
            if (rt->base_only && !dets.empty()) {
                for (auto& d : dets)
                    log_info(cfg_.task_id + " base " + rt->spec->name + " box=" +
                             std::to_string((int)d.x1) + "," + std::to_string((int)d.y1) +
                             "," + std::to_string((int)d.x2) + "," + std::to_string((int)d.y2) +
                             " score=" + std::to_string(d.score));
            }
            bool fired = false;
            if (!rt->base_only) {
                if (!dets.empty() && now_ms() - rt->last_log_ms >= 5000) {
                    rt->last_log_ms = now_ms();
                    log_info(cfg_.task_id + " model=" + rt->spec->name +
                             " detections=" + std::to_string(dets.size()));
                }
                fired = update_alarm(*rt, dets, frame, bmcv);
            }
            for (auto& d : dets) {
                if (rt->base_only) {
                    base.push_back(d);
                    persons.push_back(d);
                    continue;
                }
                base.push_back(d);
                all.push_back(d);
            }
        }
        const int64_t base_done_ms = now_ms();
        // Crop algorithms run on at most `max_crop_persons_` persons, largest
        // first.  A 4K construction feed can produce eight low-confidence
        // person boxes; classifying every one of them four times per round is
        // what multiplied the TPU demand.  Production's people threshold is
        // 0.85 and it skips frames per algorithm, so it never reaches that
        // fan-out.
        std::vector<Detection> crop_persons = base;
        std::sort(crop_persons.begin(), crop_persons.end(),
                  [](const Detection& a, const Detection& b) {
                      return (a.x2 - a.x1) * (a.y2 - a.y1) >
                             (b.x2 - b.x1) * (b.y2 - b.y1);
                  });
        if (static_cast<int>(crop_persons.size()) > max_crop_persons_)
            crop_persons.resize(static_cast<size_t>(max_crop_persons_));
        // Attribute classifiers (helmet/vest) take the top 30% of every person
        // box, matching the production crop policy validated on alarm samples.
        for (auto& rt : models_) {
            if (rt->spec->type == ModelType::YoloV10Crop) {
                if (!crop_enabled(rt->spec->name)) continue;
                // Per-algorithm cadence: skip this algorithm on rounds that are too
                // soon (production skips frames instead of queueing them).
                if (rt->min_interval_ms > 0 &&
                    now_ms() - rt->last_run_ms < rt->min_interval_ms)
                    continue;
                rt->last_run_ms = now_ms();
                int nw = rt->spec->input_w > 0 ? rt->spec->input_w : 256;
                int nh = rt->spec->input_h > 0 ? rt->spec->input_h : 256;
                std::vector<Detection> attrs;
                // Production `安全帽检测V3` has two independent class
                // thresholds: `helmet_det_threshold` and
                // `no_helmet_det_threshold`. The plugin model emits class 0 =
                // helmet, class 1 = no helmet, class 2 = person.
                float helmet_th = rt->config.conf > 0 ? rt->config.conf : 0.5f;
                float no_helmet_th = rt->config.conf_no_helmet > 0
                                         ? rt->config.conf_no_helmet
                                         : (rt->config.attr_thresh > 0
                                                ? rt->config.attr_thresh
                                                : 0.7f);
                auto prop = [](float v) {
                    if (v >= 0.0f && v <= 1.0f) return v;
                    return 1.0f / (1.0f + std::exp(-v));
                };
                for (auto& p : crop_persons) {
                    if (p.class_id != 0 || p.x2 <= p.x1 || p.y2 <= p.y1) continue;
                    // Production helmet crop: person box top 70%, like the
                    // 4-class helmet model. V3 class 0 = 未戴安全帽,
                    // class 1 = 安全帽, class 2 = 人员.
                    float pw = p.x2 - p.x1, ph = p.y2 - p.y1;
                    int cx = std::max(0, static_cast<int>(p.x1 - pw * 0.05f));
                    int cy = std::max(0, static_cast<int>(p.y1));
                    int cw = std::min(w - cx, static_cast<int>(pw * 1.05f));
                    int ch = std::min(h - cy, std::max(16, static_cast<int>(ph * 0.70f)));
                    if (cw <= 4 || ch <= 4) continue;
                    float r = std::min(static_cast<float>(nw) / cw, static_cast<float>(nh) / ch);
                    int tw = static_cast<int>(std::round(cw * r));
                    int th = static_cast<int>(std::round(ch * r));
                    sail::PaddingAtrr pad;
                    pad.set_stx((nw - tw) / 2);
                    pad.set_sty((nh - th) / 2);
                    pad.set_w(tw);
                    pad.set_h(th);
                    pad.set_r(114); pad.set_g(114); pad.set_b(114);
                    sail::BMImage* crop = pooled_frame(
                        handle, frame.format(), DATA_TYPE_EXT_1N_BYTE, nw, nh);
                    sail::BMImage* pre = pooled_frame(
                        handle, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, nw, nh);
                    if (!crop || !pre) continue;
                    if (bmcv.vpp_crop_and_resize_padding(frame, *crop, cx, cy, cw, ch,
                                                         nw, nh, pad,
                                                         bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0)
                        continue;
                    bmcv.convert_to(*crop, *pre,
                                    std::make_tuple(std::make_pair(rt->spec->alpha, rt->spec->beta),
                                                    std::make_pair(rt->spec->alpha, rt->spec->beta),
                                                    std::make_pair(rt->spec->alpha, rt->spec->beta)));
                    InferOutput out = scheduler_.infer(*rt->spec, *pre, handle, bmcv);
                    if (out.tensors.empty() || out.tensors[0].data.empty()) continue;
                    auto& t = out.tensors[0];
                    int cols = t.shape.size() >= 3 ? static_cast<int>(t.shape[2]) : 7;
                    if (cols < 7) continue;
                    bool has_helmet = false, has_no_helmet = false;
                    float best_no = 0.0f, best_helmet = 0.0f;
                    for (size_t i = 0; i + 1 < t.data.size() / cols; ++i) {
                        const float* q = t.data.data() + i * cols;
                        // Output order is [未戴安全帽, 安全帽, 人员]; the
                        // first V3 attempt had these swapped, which labelled
                        // clearly helmeted workers as violations.
                        float s_no = prop(q[4]);
                        float s_helmet = prop(q[5]);
                        if (s_helmet > best_helmet) best_helmet = s_helmet;
                        if (s_helmet >= helmet_th) has_helmet = true;
                        if (s_no >= no_helmet_th) {
                            has_no_helmet = true;
                            if (s_no > best_no) best_no = s_no;
                        }
                    }
                    if (now_ms() - rt->last_log_ms >= 2000) {
                        rt->last_log_ms = now_ms();
                        log_info(cfg_.task_id + " helmet_v3 crop=" +
                                 std::to_string(cx) + "," + std::to_string(cy) + "," +
                                 std::to_string(cw) + "," + std::to_string(ch) +
                                 " no=" + std::to_string(best_no) +
                                 " helmet=" + std::to_string(best_helmet));
                    }
                    // Independent class thresholds, as production does. The
                    // old "prefer helmet when both are hot" rule suppressed
                    // genuine no-helmet detections (office white-shirt people
                    // scored no=0.809, helmet=1.0 and were skipped).
                    if (!has_no_helmet) continue;
                    Detection d = p;
                    d.score = best_no;
                    d.class_id = rt->spec->target_id;
                    d.target = true;
                    d.label = rt->display_label.empty() ? rt->spec->report_content : rt->display_label;
                    attrs.push_back(d);
                    if (now_ms() - rt->last_log_ms >= 5000) {
                        rt->last_log_ms = now_ms();
                        log_info(cfg_.task_id + " helmet_v3 no-helmet score=" +
                                 std::to_string(best_no) + " helmet=" +
                                 std::to_string(best_helmet));
                    }
                }
                attrs = dedup_dets(std::move(attrs));
                if (!attrs.empty()) {
                    bool fired = update_alarm(*rt, attrs, frame, bmcv);
                    (void)fired;
                    for (auto& d : attrs) all.push_back(d);
                }
                continue;
            }
            if (rt->spec->type == ModelType::BoatLifeJacket) {
                if (!crop_enabled(rt->spec->name)) continue;
                // Per-algorithm cadence: skip this algorithm on rounds that are too
                // soon (production skips frames instead of queueing them).
                if (rt->min_interval_ms > 0 &&
                    now_ms() - rt->last_run_ms < rt->min_interval_ms)
                    continue;
                rt->last_run_ms = now_ms();
                // Production `MinorBoatLifeJacket` runs on a person/boat crop,
                // not on the full 4K frame. Full-frame inference classified
                // water texture as "未穿救生衣" and painted dozens of boxes.
                int nw = rt->spec->input_w > 0 ? rt->spec->input_w : 256;
                int nh = rt->spec->input_h > 0 ? rt->spec->input_h : 256;
                std::vector<Detection> attrs;
                float th = rt->config.attr_thresh > 0 ? rt->config.attr_thresh : 0.3f;
                auto prop = [](float v) {
                    if (v >= 0.0f && v <= 1.0f) return v;
                    return 1.0f / (1.0f + std::exp(-v));
                };
                for (auto& p : crop_persons) {
                    if (p.class_id != 0 || p.x2 <= p.x1 || p.y2 <= p.y1) continue;
                    float pw = p.x2 - p.x1, ph = p.y2 - p.y1;
                    int cx = std::max(0, static_cast<int>(p.x1 - pw * 0.05f));
                    int cy = std::max(0, static_cast<int>(p.y1 - ph * 0.05f));
                    int cw = std::min(w - cx, static_cast<int>(pw * 1.10f));
                    int ch = std::min(h - cy, static_cast<int>(ph * 1.10f));
                    if (cw <= 8 || ch <= 8) continue;
                    float r = std::min(static_cast<float>(nw) / cw,
                                       static_cast<float>(nh) / ch);
                    int tw = static_cast<int>(std::round(cw * r));
                    int thh = static_cast<int>(std::round(ch * r));
                    int tx = (nw - tw) / 2;
                    int ty = (nh - thh) / 2;
                    sail::PaddingAtrr pad;
                    pad.set_stx(tx); pad.set_sty(ty);
                    pad.set_w(tw); pad.set_h(thh);
                    pad.set_r(114); pad.set_g(114); pad.set_b(114);
                    sail::BMImage* crop = pooled_frame(
                        handle, frame.format(), DATA_TYPE_EXT_1N_BYTE, nw, nh);
                    sail::BMImage* pre = pooled_frame(
                        handle, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, nw, nh);
                    if (!crop || !pre) continue;
                    if (bmcv.vpp_crop_and_resize_padding(frame, *crop, cx, cy, cw, ch,
                                                         nw, nh, pad,
                                                         bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0)
                        continue;
                    bmcv.convert_to(*crop, *pre,
                                    std::make_tuple(std::make_pair(rt->spec->alpha, rt->spec->beta),
                                                    std::make_pair(rt->spec->alpha, rt->spec->beta),
                                                    std::make_pair(rt->spec->alpha, rt->spec->beta)));
                    InferOutput out = scheduler_.infer(*rt->spec, *pre, handle, bmcv);
                    if (out.tensors.empty() || out.tensors[0].data.empty()) continue;
                    auto& t = out.tensors[0];
                    int cols = t.shape.size() >= 3 ? static_cast<int>(t.shape[2]) : 10;
                    if (cols < 10) continue;
                    float max_no = 0.0f, max_life = 0.0f;
                    for (size_t i = 0; i + 1 < t.data.size() / cols; ++i) {
                        const float* q = t.data.data() + i * cols;
                        float no = prop(q[7]);       // class 3 未穿救生衣
                        float l0 = prop(q[4]);       // 救生衣1
                        float l2 = prop(q[6]);       // 救生衣2
                        float l4 = prop(q[8]);       // 救生衣3
                        if (no > max_no) max_no = no;
                        max_life = std::max(max_life, std::max(l0, std::max(l2, l4)));
                    }
                    if (max_no < th || max_no <= max_life) continue;
                    Detection d = p;
                    d.score = max_no;
                    d.class_id = rt->spec->target_id;
                    d.target = true;
                    d.label = rt->display_label.empty() ? rt->spec->report_content : rt->display_label;
                    attrs.push_back(d);
                    if (now_ms() - rt->last_log_ms >= 5000) {
                        rt->last_log_ms = now_ms();
                        log_info(cfg_.task_id + " boat_life_jacket no-lifejacket=" +
                                 std::to_string(max_no) + " lifejacket=" +
                                 std::to_string(max_life));
                    }
                }
                attrs = dedup_dets(std::move(attrs));
                if (!attrs.empty()) {
                    bool fired = update_alarm(*rt, attrs, frame, bmcv);
                    (void)fired;
                    for (auto& d : attrs) all.push_back(d);
                }
                continue;
            }
            if (rt->spec->type != ModelType::AttrClassifier) continue;
            if (!crop_enabled(rt->spec->name)) continue;
            // Per-algorithm cadence: skip this algorithm on rounds that are too
            // soon (production skips frames instead of queueing them).
            if (rt->min_interval_ms > 0 &&
                now_ms() - rt->last_run_ms < rt->min_interval_ms)
                continue;
            rt->last_run_ms = now_ms();
            int net_w = rt->spec->input_w > 0 ? rt->spec->input_w : 224;
            int net_h = rt->spec->input_h > 0 ? rt->spec->input_h : 224;
            std::vector<Detection> attrs;
            for (auto& p : crop_persons) {
                if (p.class_id != 0 || p.x2 <= p.x1 || p.y2 <= p.y1) continue;
                int cx = std::max(0, static_cast<int>(p.x1));
                int cy = std::max(0, static_cast<int>(p.y1));
                int cw = std::min(w - cx, static_cast<int>(p.x2 - p.x1));
                auto classify = [&](float crop_ratio) -> float {
                    int ch = std::max(16, static_cast<int>((p.y2 - p.y1) * crop_ratio));
                    ch = std::min(h - cy, ch);
                    if (cw <= 4 || ch <= 4) return -1;
                    float r = std::min(static_cast<float>(net_w) / cw,
                                       static_cast<float>(net_h) / ch);
                    int tw = static_cast<int>(std::round(cw * r));
                    int th = static_cast<int>(std::round(ch * r));
                    int tx = (net_w - tw) / 2;
                    int ty = (net_h - th) / 2;
                    sail::PaddingAtrr pad;
                    pad.set_stx(tx);
                    pad.set_sty(ty);
                    pad.set_w(tw);
                    pad.set_h(th);
                    pad.set_r(114);
                    pad.set_g(114);
                    pad.set_b(114);
                    sail::BMImage* crop = pooled_frame(
                        handle, frame.format(), DATA_TYPE_EXT_1N_BYTE, net_w, net_h);
                    sail::BMImage* pre = pooled_frame(
                        handle, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, net_w, net_h);
                    if (!crop || !pre) return -1;
                    if (bmcv.vpp_crop_and_resize_padding(frame, *crop, cx, cy, cw, ch,
                                                         net_w, net_h, pad,
                                                         bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0)
                        return -1;
                    bmcv.convert_to(*crop, *pre,
                                    std::make_tuple(
                                        std::make_pair(rt->spec->alpha, rt->spec->beta),
                                        std::make_pair(rt->spec->alpha, rt->spec->beta),
                                        std::make_pair(rt->spec->alpha, rt->spec->beta)));
                    InferOutput out = scheduler_.infer(*rt->spec, *pre, handle, bmcv);
                    if (static_cast<int>(out.tensors.size()) <= rt->spec->attr_head) return -1;
                    auto& t = out.tensors[rt->spec->attr_head];
                    if (t.data.empty()) return -1;
                    int n = static_cast<int>(t.data.size());
                    float sum = 0;
                    for (int i = 0; i < n; ++i) sum += std::exp(t.data[i]);
                    int ac = std::min(rt->spec->attr_class, n - 1);
                    return std::exp(t.data[std::max(0, ac)]) / (sum + 1e-9f);
                };
                float need = rt->config.attr_thresh > 0 ? rt->config.attr_thresh : rt->spec->attr_thresh;
                float prob = classify(rt->spec->name == "helmet" ? 0.70f : 0.30f);
                // Helmet: a second full-person crop recovers heads clipped by
                // the top-70% box, but only runs for uncertain cases.
                if (prob >= 0 && prob < need && rt->spec->name == "helmet") {
                    float alt = classify(1.0f);
                    if (alt > prob) prob = alt;
                }
                if (prob < need) continue;
                Detection d = p;
                d.score = prob;
                d.class_id = 0;
                d.target = true;
                d.label = rt->display_label.empty() ? rt->spec->report_content : rt->display_label;
                attrs.push_back(d);
            }
            attrs = dedup_dets(std::move(attrs));
            if (!attrs.empty() && now_ms() - rt->last_log_ms >= 5000) {
                rt->last_log_ms = now_ms();
                log_info(cfg_.task_id + " model=" + rt->spec->name +
                         " detections=" + std::to_string(attrs.size()));
            }
            bool fired = update_alarm(*rt, attrs, frame, bmcv);
            for (auto& d : attrs) {
                all.push_back(d);
            }
        }
        // Behaviour model (smoking/calling/phone) runs on the full person crop.
        for (auto& rt : models_) {
            if (rt->spec->type != ModelType::YoloV5Crop) continue;
            if (!crop_enabled(rt->spec->name)) continue;
            // Per-algorithm cadence: skip this algorithm on rounds that are too
            // soon (production skips frames instead of queueing them).
            if (rt->min_interval_ms > 0 &&
                now_ms() - rt->last_run_ms < rt->min_interval_ms)
                continue;
            rt->last_run_ms = now_ms();
            int net_w = rt->spec->input_w > 0 ? rt->spec->input_w : 256;
            int net_h = rt->spec->input_h > 0 ? rt->spec->input_h : 256;
            std::vector<Detection> beh;
            for (auto& p : crop_persons) {
                if (p.class_id != 0 || p.x2 <= p.x1 || p.y2 <= p.y1) continue;
                int cx = std::max(0, static_cast<int>(p.x1));
                int cy = std::max(0, static_cast<int>(p.y1));
                int cw = std::min(w - cx, static_cast<int>(p.x2 - p.x1));
                int ch = std::min(h - cy, static_cast<int>(p.y2 - p.y1));
                // People boxes can legitimately be 30%+ of a 4K frame; the
                // old head-box size filter skipped the foreground smoker.
                if (cw < 16 || ch < 16) continue;
                // Production behaviour crop is the majorId=1 person box,
                // not a head box. A head-only crop missed the cigarette hand
                // and made a bent background worker look like a smoker.
                {
                    int bw = std::max(8, cw), bh = std::max(8, ch);
                    cx = std::max(0, cx - static_cast<int>(bw * 0.05f));
                    cy = std::max(0, cy - static_cast<int>(bh * 0.05f));
                    cw = std::min(w - cx, static_cast<int>(bw * 1.10f));
                    ch = std::min(h - cy, static_cast<int>(bh * 1.10f));
                }
                if (cw <= 8 || ch <= 8) continue;
                float r = std::min(static_cast<float>(net_w) / cw,
                                   static_cast<float>(net_h) / ch);
                int tw = static_cast<int>(std::round(cw * r));
                int th = static_cast<int>(std::round(ch * r));
                int tx = (net_w - tw) / 2;
                int ty = (net_h - th) / 2;
                sail::PaddingAtrr pad;
                pad.set_stx(tx);
                pad.set_sty(ty);
                pad.set_w(tw);
                pad.set_h(th);
                pad.set_r(114);
                pad.set_g(114);
                pad.set_b(114);
                sail::BMImage* crop = pooled_frame(
                    handle, frame.format(), DATA_TYPE_EXT_1N_BYTE, net_w, net_h);
                sail::BMImage* pre = pooled_frame(
                    handle, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32, net_w, net_h);
                if (!crop || !pre) continue;
                if (bmcv.vpp_crop_and_resize_padding(frame, *crop, cx, cy, cw, ch,
                                                     net_w, net_h, pad,
                                                     bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0)
                    continue;
                bmcv.convert_to(*crop, *pre,
                                std::make_tuple(
                                    std::make_pair(rt->spec->alpha, rt->spec->beta),
                                    std::make_pair(rt->spec->alpha, rt->spec->beta),
                                    std::make_pair(rt->spec->alpha, rt->spec->beta)));
                InferOutput out = scheduler_.infer(*rt->spec, *pre, handle, bmcv);
                auto ds = postprocess_behavior(out, *rt, cw, ch, r, tx, ty);
                log_info(cfg_.task_id + " behavior infer ds=" + std::to_string(ds.size()));
                float best[3] = {0.0f, 0.0f, 0.0f};
                for (auto& d : ds) {
                    if (d.class_id >= 0 && d.class_id < 3)
                        best[d.class_id] = std::max(best[d.class_id], d.score);
                }
                float cls_th[3] = {rt->config.smoking_thresh,
                                   rt->config.calling_thresh,
                                   rt->config.phone_thresh};
                bool en[3] = {rt->config.behavior_enable_smoking,
                              rt->config.behavior_enable_calling,
                              rt->config.behavior_enable_playing};
                int bc = -1;
                float best_score = 0;
                for (int c = 0; c < 3; ++c) {
                    if (!en[c] || best[c] < cls_th[c]) continue;
                    if (best[c] > best_score) { best_score = best[c]; bc = c; }
                }
                if (best_score > 0)
                    log_info(cfg_.task_id + " behavior best=" + std::to_string(best_score) +
                             " class=" + std::to_string(bc));
                if (bc < 0) continue;
                Detection d = p;
                d.score = best_score;
                d.class_id = bc;
                d.target = true;
                d.label = rt->display_label.empty() ? rt->spec->report_content : rt->display_label;
                beh.push_back(d);
            }
            beh = dedup_dets(std::move(beh));
            // Smoking/calling is a per-person event. The behaviour classifier
            // also fires on background workers whose crop happens to contain a
            // hand near the face. Keep the most plausible foreground target
            // (score weighted by crop area) instead of alarming every head.
            if (beh.size() > 1) {
                size_t best_i = 0;
                float best_w = -1.0f;
                for (size_t i = 0; i < beh.size(); ++i) {
                    float area = std::max(1.0f, (beh[i].x2 - beh[i].x1) *
                                                     (beh[i].y2 - beh[i].y1));
                    float w = beh[i].score * std::sqrt(area);
                    if (w > best_w) { best_w = w; best_i = i; }
                }
                Detection keep = beh[best_i];
                beh.clear();
                beh.push_back(keep);
            }
            if (!beh.empty() && now_ms() - rt->last_log_ms >= 5000) {
                rt->last_log_ms = now_ms();
                log_info(cfg_.task_id + " model=" + rt->spec->name +
                         " detections=" + std::to_string(beh.size()));
            }
            bool fired = update_alarm(*rt, beh, frame, bmcv);
            (void)fired;
            for (auto& d : beh) {
                all.push_back(d);
            }
        }
        const int64_t crop_done_ms = now_ms();
        size_t dets_ms_prof = 0, persons_ms_prof = 0;
        {
            std::vector<Detection> prev;
            int64_t prev_ms = 0;
            {
                std::lock_guard<std::mutex> lk(det_mutex_);
                prev = dets_;
                prev_ms = dets_ms_;
            }
            // Boxes belong to the frame that was actually inferred, not to
            // "now" when inference finished.  Using completion time made the
            // preview extrapolate from a timestamp newer than the box, so the
            // drawn box lagged the person (merge was worst under load).
            const int64_t tnow = frame_ts;
            float dt = std::max(0.05f, (tnow - prev_ms) / 1000.0f);
            for (auto& d : all) {
                float best = 0.15f;
                const Detection* match = nullptr;
                for (auto& p : prev) {
                    if (p.class_id != d.class_id || p.label != d.label) continue;
                    float ix1 = std::max(d.x1, p.x1), iy1 = std::max(d.y1, p.y1);
                    float ix2 = std::min(d.x2, p.x2), iy2 = std::min(d.y2, p.y2);
                    if (ix2 <= ix1 || iy2 <= iy1) continue;
                    float inter = (ix2 - ix1) * (iy2 - iy1);
                    float uni = (d.x2 - d.x1) * (d.y2 - d.y1) +
                                (p.x2 - p.x1) * (p.y2 - p.y1) - inter;
                    float iou = uni > 0 ? inter / uni : 0;
                    if (iou > best) { best = iou; match = &p; }
                }
                if (match) {
                    d.vx = (((d.x1 + d.x2) - (match->x1 + match->x2)) * 0.5f) / dt;
                    d.vy = (((d.y1 + d.y2) - (match->y1 + match->y2)) * 0.5f) / dt;
                }
            }
            // No cross-frame "active" rescue: a box/label is drawn only for
            // the detection snapshot that actually contains it.  Keeping the
            // last confirmed box for a TTL is exactly what left a ghost on the
            // ROI edge after the person had already walked away.
            // Final safety net: every algorithm's box must have its center
            // inside the task ROI.
            all = filter_roi_polys(roi_polygons_, all, w, h);
            dets_ms_prof = all.size();
            persons_ms_prof = persons.size();
            std::lock_guard<std::mutex> lk(det_mutex_);            dets_ = std::move(all);
            dets_persons_ = std::move(persons);
            dets_ms_ = tnow;
            dets_history_.push_back({tnow, dets_, dets_persons_});
            while (dets_history_.size() > 20) dets_history_.pop_front();
        }
        if (profiling()) {
            const int64_t t = now_ms();
            if (t - last_prof_ms_ >= 2000) {
                last_prof_ms_ = t;
                log_info(cfg_.task_id + " detect round ms=" +
                         std::to_string(t - prof_t0) + " models=" +
                         std::to_string(models_.size()) + " base=" +
                         std::to_string(persons_ms_prof) + " all=" +
                         std::to_string(dets_ms_prof) + " res=" +
                         std::to_string(w) + "x" + std::to_string(h) +
                         " base_ms=" + std::to_string(base_done_ms - prof_t0) +
                         " crop_ms=" + std::to_string(crop_done_ms - base_done_ms) +
                         " post_ms=" + std::to_string(now_ms() - crop_done_ms));
            }
        }
    }
    std::vector<Detection> postprocess_yolo(const InferOutput& out, ModelRuntime& rt,
                                            int w, int h, float ratio, int tx, int ty) {
        if (out.tensors.empty()) return {};
        auto& t = out.tensors[0];
        auto& shape = t.shape;
        if (shape.size() != 3 || t.data.empty()) return {};
        bool transposed = shape[1] < shape[2];
        int box_num = transposed ? shape[2] : shape[1];
        int nout = transposed ? shape[1] : shape[2];
        int nc = nout - 4;
        if (nc <= 0) return {};
        auto at = [&](int i, int c) -> float {
            return transposed ? t.data[static_cast<size_t>(c) * box_num + i]
                              : t.data[static_cast<size_t>(i) * nout + c];
        };
        std::vector<Detection> raw;
        for (int i = 0; i < box_num; ++i) {
            float x = at(i, 0), y = at(i, 1), bw = at(i, 2), bh = at(i, 3);
            for (int c = 0; c < nc; ++c) {
                float s = at(i, 4 + c);
                if (s < rt.config.conf) continue;
                Detection d;
                d.score = s;
                d.class_id = c;
                d.target = (c == rt.spec->target_id || c == rt.spec->target_id2);
                d.label = class_label(*rt.spec, c);
                d.x1 = (x - bw / 2 - tx) / ratio;
                d.y1 = (y - bh / 2 - ty) / ratio;
                d.x2 = (x + bw / 2 - tx) / ratio;
                d.y2 = (y + bh / 2 - ty) / ratio;
                clip_box(d, w, h);
                if (d.x2 > d.x1 && d.y2 > d.y1) raw.push_back(d);
            }
        }
        return nms(raw, rt.config.nms);
    }
    std::vector<Detection> postprocess_yolov5(const InferOutput& out, ModelRuntime& rt,
                                              int w, int h, float ratio, int tx, int ty) {
        if (out.tensors.empty()) return {};
        auto& t = out.tensors[0];
        auto& shape = t.shape;
        if (shape.size() != 3 || shape[2] < 5 || t.data.empty()) return {};
        int n = shape[1];
        int cols = shape[2];
        bool no_class = (cols == 5);
        int nc = no_class ? 1 : cols - 5;
        auto sig = [](float v) { return 1.0f / (1.0f + std::exp(-v)); };
        std::vector<Detection> raw;
        for (int i = 0; i < n; ++i) {
            const float* q = t.data.data() + static_cast<size_t>(i) * cols;
            float obj = q[4];
            if (obj < rt.config.conf) continue;
            float x1 = q[0], y1 = q[1], x2 = q[2], y2 = q[3];
            if (x2 < x1) std::swap(x1, x2);
            if (y2 < y1) std::swap(y1, y2);
            for (int c = 0; c < nc; ++c) {
                float s = obj * (no_class ? 1.0f : sig(q[5 + c]));
                if (s < rt.config.conf) continue;
                Detection d;
                d.score = s;
                d.class_id = c;
                d.target = (c == rt.spec->target_id || c == rt.spec->target_id2);
                if (rt.spec->name == "smoking_call") {
                    d.target = (c == 0 && rt.config.behavior_enable_smoking) ||
                               (c == 1 && rt.config.behavior_enable_calling) ||
                               (c == 2 && rt.config.behavior_enable_playing);
                }
                d.label = class_label(*rt.spec, c);
                d.x1 = (x1 - tx) / ratio;
                d.y1 = (y1 - ty) / ratio;
                d.x2 = (x2 - tx) / ratio;
                d.y2 = (y2 - ty) / ratio;
                clip_box(d, w, h);
                if (d.x2 > d.x1 && d.y2 > d.y1) raw.push_back(d);
            }
        }
        return nms(raw, rt.config.nms);
    }
    // Production behaviour model is YOLOv10-style: 4 box + 5 class scores,
    // no objectness column. Parsing it as YOLOv5 previously shifted every
    // class by one and dropped class4.
    std::vector<Detection> postprocess_behavior(const InferOutput& out, ModelRuntime& rt,
                                                int w, int h, float ratio, int tx, int ty) {
        if (out.tensors.empty()) return {};
        auto& t = out.tensors[0];
        auto& shape = t.shape;
        if (shape.size() != 3 || shape[2] < 9 || t.data.empty()) return {};
        int n = shape[1], cols = shape[2];
        int nc = std::min(5, cols - 4);
        auto prop = [](float v) {
            if (v >= 0.0f && v <= 1.0f) return v;
            return 1.0f / (1.0f + std::exp(-v));
        };
        std::vector<Detection> raw;
        for (int i = 0; i < n; ++i) {
            const float* q = t.data.data() + static_cast<size_t>(i) * cols;
            float x1 = q[0], y1 = q[1], x2 = q[2], y2 = q[3];
            if (x2 < x1) std::swap(x1, x2);
            if (y2 < y1) std::swap(y1, y2);
            for (int c = 0; c < nc; ++c) {
                float s = prop(q[4 + c]);
                if (s < rt.config.conf) continue;
                bool en = (c == 0 && rt.config.behavior_enable_smoking) ||
                          (c == 1 && rt.config.behavior_enable_calling) ||
                          (c == 2 && rt.config.behavior_enable_playing);
                if (!en) continue;
                Detection d;
                d.score = s;
                d.class_id = c;
                d.target = true;
                d.label = class_label(*rt.spec, c);
                d.x1 = (x1 - tx) / ratio;
                d.y1 = (y1 - ty) / ratio;
                d.x2 = (x2 - tx) / ratio;
                d.y2 = (y2 - ty) / ratio;
                clip_box(d, w, h);
                if (d.x2 > d.x1 && d.y2 > d.y1) raw.push_back(d);
            }
        }
        return nms(raw, rt.config.nms);
    }
    // YOLOv8 anchor-free head: 4 box logits + N class logits, grid 32/16/8.
    std::vector<Detection> postprocess_yolov8(const InferOutput& out, ModelRuntime& rt,
                                               int w, int h, float ratio, int tx, int ty) {
        if (out.tensors.empty()) return {};
        auto& t = out.tensors[0];
        auto& shape = t.shape;
        if (shape.size() != 3 || shape[2] < 6 || t.data.empty()) return {};
        int n = shape[1], cols = shape[2];
        int nc = cols - 4;
        auto sig = [](float v) { return 1.0f / (1.0f + std::exp(-v)); };
        struct S { int gw, gh, stride; };
        S scales[3] = {{32, 32, 8}, {16, 16, 16}, {8, 8, 32}};
        std::vector<Detection> raw;
        int start = 0;
        for (int s = 0; s < 3; ++s) {
            int cells = scales[s].gw * scales[s].gh;
            for (int idx = 0; idx < cells; ++idx) {
                const float* q = t.data.data() + static_cast<size_t>(start + idx) * cols;
                float cx = (sig(q[0]) * 2.0f - 0.5f + (idx % scales[s].gw)) * scales[s].stride;
                float cy = (sig(q[1]) * 2.0f - 0.5f + (idx / scales[s].gw)) * scales[s].stride;
                float bw = std::pow(sig(q[2]) * 2.0f, 2.0f) * scales[s].stride;
                float bh = std::pow(sig(q[3]) * 2.0f, 2.0f) * scales[s].stride;
                float best = 0;
                int bc = 0;
                for (int c = 0; c < nc && c < 32; ++c) {
                    float v = sig(q[4 + c]);
                    if (v > best) { best = v; bc = c; }
                }
                if (best < rt.config.conf) continue;
                if (bc != rt.spec->target_id && bc != rt.spec->target_id2) continue;
                Detection d;
                d.score = best;
                d.class_id = bc;
                d.target = true;
                d.label = class_label(*rt.spec, bc);
                d.x1 = (cx - bw / 2 - tx) / ratio;
                d.y1 = (cy - bh / 2 - ty) / ratio;
                d.x2 = (cx + bw / 2 - tx) / ratio;
                d.y2 = (cy + bh / 2 - ty) / ratio;
                clip_box(d, w, h);
                if (d.x2 > d.x1 && d.y2 > d.y1) raw.push_back(d);
            }
            start += cells;
        }
        return nms(raw, rt.config.nms);
    }
    // YOLOv10 helmet V3 head: 4 box + 3 class columns, boxes are usually
    // expressed in a 4x input-grid; normalize when values exceed input size.
    std::vector<Detection> postprocess_yolov10(const InferOutput& out, ModelRuntime& rt,
                                                int w, int h, float ratio, int tx, int ty) {
        if (out.tensors.empty()) return {};
        auto& t = out.tensors[0];
        if (t.shape.size() != 3 || t.shape[2] < 7 || t.data.empty()) return {};
        int n = t.shape[1], cols = t.shape[2];
        auto sig = [](float v) { return 1.0f / (1.0f + std::exp(-v)); };
        std::vector<Detection> raw;
        for (int i = 0; i < n; ++i) {
            const float* q = t.data.data() + static_cast<size_t>(i) * cols;
            float x1 = q[0], y1 = q[1], x2 = q[2], y2 = q[3];
            if (x2 < x1) std::swap(x1, x2);
            if (y2 < y1) std::swap(y1, y2);
            // Keep the exported V10 box coordinates as-is; the model emits
            // coordinates in the source-image scale (not 256-grid units).
            float best = 0;
            int bc = 0;
            for (int c = 0; c < cols - 4 && c < 8; ++c) {
                float v = sig(q[4 + c]);
                if (v > best) { best = v; bc = c; }
            }
            if (best < rt.config.conf) continue;
            if (bc != rt.spec->target_id && bc != rt.spec->target_id2) continue;
            Detection d;
            d.score = best;
            d.class_id = bc;
            d.target = true;
            d.label = class_label(*rt.spec, bc);
            d.x1 = (x1 - tx) / ratio;
            d.y1 = (y1 - ty) / ratio;
            d.x2 = (x2 - tx) / ratio;
            d.y2 = (y2 - ty) / ratio;
            clip_box(d, w, h);
            if (d.x2 > d.x1 && d.y2 > d.y1) raw.push_back(d);
        }
        return nms(raw, rt.config.nms);
    }
    // Crowd/head heatmap: output0 is 96x160x2 class logits, output1 is offsets.
    std::vector<Detection> postprocess_crowd(const InferOutput& out, ModelRuntime& rt,
                                              int w, int h, float ratio, int tx, int ty) {
        if (out.tensors.size() < 1 || out.tensors[0].shape.size() != 3) return {};
        auto& t0 = out.tensors[0];
        int n = t0.shape[1], cols = t0.shape[2];
        if (cols < 2 || n < 16) return {};
        int gw = 160, gh = (n + gw - 1) / gw;
        auto sig = [](float v) { return 1.0f / (1.0f + std::exp(-v)); };
        std::vector<Detection> raw;
        for (int i = 0; i < n; ++i) {
            const float* q = t0.data.data() + static_cast<size_t>(i) * cols;
            float s = sig(q[1] - q[0]);
            if (s < rt.config.conf) continue;
            int px = i % gw, py = i / gw;
            float ox = 0.5f, oy = 0.5f;
            if (out.tensors.size() > 1 && out.tensors[1].data.size() >= static_cast<size_t>(i * 2 + 2)) {
                ox += out.tensors[1].data[i * 2];
                oy += out.tensors[1].data[i * 2 + 1];
            }
            float cx = (px + ox) * (static_cast<float>(rt.spec->input_w) / gw);
            float cy = (py + oy) * (static_cast<float>(rt.spec->input_h) / gh);
            Detection d;
            d.score = s;
            d.class_id = 1;
            d.target = true;
            d.label = class_label(*rt.spec, 1);
            d.x1 = (cx - 8 - tx) / ratio;
            d.y1 = (cy - 8 - ty) / ratio;
            d.x2 = (cx + 8 - tx) / ratio;
            d.y2 = (cy + 8 - ty) / ratio;
            clip_box(d, w, h);
            if (d.x2 > d.x1 && d.y2 > d.y1) raw.push_back(d);
        }
        return nms(raw, 0.2f);
    }
    std::vector<Detection> postprocess_damo(const InferOutput& out, ModelRuntime& rt,
                                            int w, int h, float ratio, int tx, int ty) {
        const TensorData* scores = nullptr;
        const TensorData* boxes = nullptr;
        for (auto& t : out.tensors) {
            if (t.shape.size() == 3 && t.shape[2] == 4) boxes = &t;
            else if (t.shape.size() == 3) scores = &t;
        }
        if (!scores || !boxes || boxes->data.empty() || scores->data.empty()) return {};
        int n = scores->shape[1];
        int nc = scores->shape[2];
        if (n <= 0 || nc <= 0) return {};
        std::vector<Detection> raw;
        for (int i = 0; i < n; ++i) {
            float bx1 = boxes->data[i * 4 + 0];
            float by1 = boxes->data[i * 4 + 1];
            float bx2 = boxes->data[i * 4 + 2];
            float by2 = boxes->data[i * 4 + 3];
            for (int c = 0; c < nc; ++c) {
                float s = scores->data[static_cast<size_t>(i) * nc + c];
                if (s < rt.config.conf) continue;
                Detection d;
                d.score = s;
                d.class_id = c;
                d.target = (c == rt.spec->target_id || c == rt.spec->target_id2);
                d.label = class_label(*rt.spec, c);
                d.x1 = (bx1 - tx) / ratio;
                d.y1 = (by1 - ty) / ratio;
                d.x2 = (bx2 - tx) / ratio;
                d.y2 = (by2 - ty) / ratio;
                clip_box(d, w, h);
                if (d.x2 > d.x1 && d.y2 > d.y1) raw.push_back(d);
            }
        }
        return nms(raw, rt.config.nms);
    }
    static std::string class_label(const ModelSpec& spec, int c) {
        if (c >= 0 && c < static_cast<int>(spec.class_names.size()))
            return spec.class_names[c];
        return spec.name;
    }
    static void clip_box(Detection& d, int w, int h) {
        d.x1 = std::max(0.0f, std::min(d.x1, static_cast<float>(w - 1)));
        d.y1 = std::max(0.0f, std::min(d.y1, static_cast<float>(h - 1)));
        d.x2 = std::max(0.0f, std::min(d.x2, static_cast<float>(w - 1)));
        d.y2 = std::max(0.0f, std::min(d.y2, static_cast<float>(h - 1)));
    }
    std::vector<Detection> nms(std::vector<Detection>& raw, float nms_thresh) {
        std::sort(raw.begin(), raw.end(), [](const Detection& a, const Detection& b) {
            return a.score > b.score;
        });
        std::vector<Detection> keep;
        std::vector<bool> removed(raw.size(), false);
        for (size_t i = 0; i < raw.size(); ++i) {
            if (removed[i]) continue;
            keep.push_back(raw[i]);
            for (size_t j = i + 1; j < raw.size(); ++j) {
                if (removed[j] || raw[i].class_id != raw[j].class_id) continue;
                float xx1 = std::max(raw[i].x1, raw[j].x1);
                float yy1 = std::max(raw[i].y1, raw[j].y1);
                float xx2 = std::min(raw[i].x2, raw[j].x2);
                float yy2 = std::min(raw[i].y2, raw[j].y2);
                float inter = std::max(0.0f, xx2 - xx1) * std::max(0.0f, yy2 - yy1);
                float a1 = (raw[i].x2 - raw[i].x1) * (raw[i].y2 - raw[i].y1);
                float a2 = (raw[j].x2 - raw[j].x1) * (raw[j].y2 - raw[j].y1);
                float iou = inter / (a1 + a2 - inter + 1e-6f);
                if (iou > nms_thresh) removed[j] = true;
            }
        }
        return keep;
    }
    static bool point_in_poly(const std::vector<std::pair<float, float>>& poly,
                              float x, float y) {
        bool inside = false;
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
            float xi = poly[i].first, yi = poly[i].second;
            float xj = poly[j].first, yj = poly[j].second;
            bool intersect = ((yi > y) != (yj > y)) &&
                             (x < (xj - xi) * (y - yi) / (yj - yi + 1e-9f) + xi);
            if (intersect) inside = !inside;
        }
        return inside;
    }
    std::vector<Detection> filter_roi_polys(
        const std::vector<std::vector<std::pair<float, float>>>& polys,
        const std::vector<Detection>& dets, int w, int h) {
        if (polys.empty()) return dets;
        std::vector<Detection> out;
        for (auto& d : dets) {
            float cx = (d.x1 + d.x2) * 0.5f / std::max(1, w);
            float cy = (d.y1 + d.y2) * 0.5f / std::max(1, h);
            for (auto& poly : polys) {
                if (point_in_poly(poly, cx, cy)) {
                    out.push_back(d);
                    break;
                }
            }
        }
        return out;
    }
    std::vector<Detection> filter_roi(const std::vector<Detection>& dets, ModelRuntime& rt,
                                      int w, int h) {
        // Production's SystemAlarmFilter checks the box center against the
        // task regions (isBoxCenterInRegions / BoxInRoiRegion).  Crop
        // algorithms (V3/vest/boat/behavior) used to skip this check and only
        // the base person detector was filtered, so targets outside the green
        // ROI still got boxes/labels/alarms.
        const auto& polys = !rt.config.polygons.empty() ? rt.config.polygons
                                                        : roi_polygons_;
        return filter_roi_polys(polys, dets, w, h);
    }
    std::vector<Detection> dedup_dets(std::vector<Detection> in, float thr = 0.5f) {
        std::sort(in.begin(), in.end(),
                  [](const Detection& a, const Detection& b) { return a.score > b.score; });
        std::vector<Detection> out;
        for (auto& d : in) {
            bool keep = true;
            for (auto& k : out) {
                if (k.class_id != d.class_id || k.label != d.label) continue;
                float ix1 = std::max(d.x1, k.x1), iy1 = std::max(d.y1, k.y1);
                float ix2 = std::min(d.x2, k.x2), iy2 = std::min(d.y2, k.y2);
                if (ix2 <= ix1 || iy2 <= iy1) continue;
                float inter = (ix2 - ix1) * (iy2 - iy1);
                float uni = (d.x2 - d.x1) * (d.y2 - d.y1) +
                            (k.x2 - k.x1) * (k.y2 - k.y1) - inter;
                if (uni > 0 && inter / uni > thr) { keep = false; break; }
            }
            if (keep) out.push_back(d);
        }
        return out;
    }    cv::Mat to_mat(sail::BMImage& frame, sail::Bmcv& bmcv) {
        sail::BMImage bgr = bmcv.convert_format(frame, FORMAT_BGR_PLANAR);
        cv::Mat mat;
        sail::Bmcv::bm_image_to_mat(bgr, mat);
        return mat.clone();
    }
    void draw_roi_cv(cv::Mat& canvas) {
        std::vector<std::vector<std::pair<float, float>>> rois;
        {
            std::lock_guard<std::mutex> lk(roi_mu_);
            rois = roi_polygons_;
        }
        if (rois.empty() || canvas.empty()) return;
        std::vector<std::vector<cv::Point>> polys;
        for (auto& roi : rois) {
            std::vector<cv::Point> pts;
            for (auto& p : roi) {
                int x = static_cast<int>(std::lround(
                    std::max(0.0f, std::min(1.0f, p.first)) * (canvas.cols - 1)));
                int y = static_cast<int>(std::lround(
                    std::max(0.0f, std::min(1.0f, p.second)) * (canvas.rows - 1)));
                pts.emplace_back(x, y);
            }
            if (pts.size() >= 3) polys.push_back(std::move(pts));
        }
        if (polys.empty()) return;
        int thickness = std::max(2, std::min(6, canvas.cols / 320));
        cv::polylines(canvas, polys, true, cv::Scalar(0, 255, 0), thickness,
                      cv::LINE_AA);
    }
    void draw_roi_bm(sail::Bmcv& bmcv, sail::BMImage& img, int w, int h) {
        std::vector<std::vector<std::pair<float, float>>> rois;
        {
            std::lock_guard<std::mutex> lk(roi_mu_);
            rois = roi_polygons_;
        }
        if (rois.empty() || w <= 1 || h <= 1) return;
        std::vector<std::vector<std::pair<int, int>>> polys;
        for (auto& roi : rois) {
            std::vector<std::pair<int, int>> pts;
            for (auto& p : roi) {
                int x = static_cast<int>(std::lround(
                    std::max(0.0f, std::min(1.0f, p.first)) * (w - 1)));
                int y = static_cast<int>(std::lround(
                    std::max(0.0f, std::min(1.0f, p.second)) * (h - 1)));
                pts.emplace_back(x, y);
            }
            if (pts.size() >= 3) polys.push_back(std::move(pts));
        }
        if (polys.empty()) return;
        int thickness = std::max(2, std::min(6, w / 320));
        try {
            bmcv.polylines(img, polys, true, std::make_tuple(0, 255, 0),
                           thickness);
        } catch (const std::exception& e) {
            log_warn(cfg_.task_id + " roi draw: " + e.what());
        }
    }
    std::vector<unsigned char> render_jpeg(sail::BMImage& frame, sail::Bmcv& bmcv, int out_w,
                                           int out_h, const std::vector<Detection>& dets,
                                           const std::string& alarm_out = "") {
        cv::Mat mat = to_mat(frame, bmcv);
        if (mat.empty()) return {};
        std::lock_guard<std::mutex> font_lock(font_mutex());
        cv::Mat canvas;
        if (mat.cols != out_w || mat.rows != out_h) {
            cv::resize(mat, canvas, cv::Size(out_w, out_h));
        } else {
            canvas = mat;
        }
        draw_roi_cv(canvas);
        float sx = static_cast<float>(out_w) / std::max(1, frame_w_);
        float sy = static_cast<float>(out_h) / std::max(1, frame_h_);
        std::vector<std::pair<std::string, cv::Point>> label_draws;
        std::vector<std::pair<cv::Mat, cv::Point>> plate_draws;
        std::vector<cv::Rect> label_rects;
        for (auto& d : dets) {
            // Preview must show alarm targets only. Non-target classes from a
            // generic detector (cars/people/boats) otherwise paint dozens of
            // meaningless green boxes over the scene.
            if (!d.target && !d.pending && !d.confirmed) continue;
            cv::Scalar color = d.pending ? cv::Scalar(255, 0, 0) : (d.target ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0));
            cv::rectangle(canvas,
                          cv::Rect(static_cast<int>(d.x1 * sx), static_cast<int>(d.y1 * sy),
                                   static_cast<int>((d.x2 - d.x1) * sx),
                                   static_cast<int>((d.y2 - d.y1) * sy)),
                          color, 2);
            if (d.pending) {
                std::string cd = std::to_string(d.countdown_s) + "s";
                cv::putText(canvas, cd,
                            cv::Point(static_cast<int>(d.x1 * sx) + 4,
                                      static_cast<int>(d.y1 * sy) + 22),
                            cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 0, 0), 2);
                // Blue pending stage: countdown only, no alarm label until
                // the target is confirmed.
                continue;
            }
            // Keep the label visible for confirmed targets even when the
            // upload interval suppresses a new alarm; hiding it made preview
            // show bare red boxes.
            // Draw labels on-device. Relying on the optional Pillow worker
            // made alarm images show a red box with no label whenever the
            // worker was unavailable.
            // Preview/alarm labels never show a confidence score.
            std::string label = d.label;
            int bx = static_cast<int>(d.x1 * sx);
            int by = static_cast<int>(d.y1 * sy);
            int bw = static_cast<int>((d.x2 - d.x1) * sx);
            int bh = static_cast<int>((d.y2 - d.y1) * sy);
            auto plate_it = label_plates_.find(d.label);
            if (plate_it != label_plates_.end()) {
                cv::Mat cls = plate_it->second;
                int bw = cls.cols;
                int bh = cls.rows;
                cv::Mat badge = cls;
                int lx = bx;
                // Production-style vertical stack: first label bottom touches
                // the red box top, subsequent labels stack directly above it.
                int ly = by - bh;
                for (int guard = 0; guard < 12; ++guard) {
                    cv::Rect lr(lx, ly, bw, bh);
                    bool hit = false;
                    for (auto& r : label_rects)
                        if ((lr & r).area() > 0) { hit = true; break; }
                    if (!hit) break;
                    ly -= bh;
                }
                if (ly < 0) ly = by + bh + 2;
                if (lx + bw > canvas.cols) lx = std::max(0, canvas.cols - bw);
                if (ly + bh > canvas.rows) ly = std::max(0, canvas.rows - bh);
                label_rects.emplace_back(lx, ly, bw, bh);
                plate_draws.emplace_back(badge, cv::Point(lx, ly));
                continue;
            }
            int label_h = 26, label_w = 0, baseline = 0;
            if (font_ok_ && ft2_) {
                try {
                    auto ts = ft2_->getTextSize(label, 30, 1, &baseline);
                    label_w = ts.width + 12;
                    label_h = ts.height + 8;
                } catch (...) {
                    font_ok_ = false;
                }
            }
            if (!font_ok_) {
                auto ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.55, 1,
                                          &baseline);
                label_w = ts.width + 10;
                label_h = ts.height + 8;
            }
            int lx = bx;
            int ly = by - label_h;
            for (int guard = 0; guard < 12; ++guard) {
                cv::Rect lr(lx, ly, label_w, label_h);
                bool hit = false;
                for (auto& r : label_rects)
                    if ((lr & r).area() > 0) { hit = true; break; }
                if (!hit) break;
                ly -= label_h;
            }
            if (ly < 0) ly = by + bh + 2;
            if (ly + label_h > canvas.rows) ly = std::max(0, canvas.rows - label_h);
            if (lx + label_w > canvas.cols) lx = std::max(0, canvas.cols - label_w);
            label_rects.emplace_back(lx, ly, label_w, label_h);
            cv::rectangle(canvas, cv::Rect(lx, ly, label_w, label_h),
                          cv::Scalar(0, 0, 255), cv::FILLED);
            if (font_ok_ && ft2_) {
                try {
                    label_draws.emplace_back(label, cv::Point(lx + 4, ly + label_h - 5));
                } catch (const std::exception& e) {
                    log_warn(cfg_.task_id + " label getTextSize: " + e.what());
                }
            }
            if (!font_ok_) {
                label_draws.emplace_back(label, cv::Point(lx + 5, ly + label_h - 5));
            }
            // Temporary visual probe: if this is visible but the FreeType text
            // above is not, the issue is in the FreeType label path.
            // label_draws.emplace_back("TEST", cv::Point(lx + 4, ly + 20));
        }
        // Draw all label text after all boxes/plates so a later detection's
        // red plate cannot cover an earlier label's white text.
        for (auto& pd : plate_draws) {
            pd.first.copyTo(canvas(cv::Rect(pd.second.x, pd.second.y,
                                            pd.first.cols, pd.first.rows)));
        }
        if (!plate_draws.empty()) {
            log_info(cfg_.task_id + " plate_draws=" + std::to_string(plate_draws.size()));
        }
        for (auto& ld : label_draws) {
            if (font_ok_ && ft2_) {
                try {
                    int base = 0;
                    auto ts = ft2_->getTextSize(ld.first, 30, 1, &base);
                    int bw = ts.width + 8;
                    int bh = ts.height + 6;
                    cv::Mat badge(bh, bw, CV_8UC3, cv::Scalar(0, 0, 255));
                    // OpenCV's FreeType2 putText uses the TOP-LEFT corner as the
                    // origin (see task_bar_mat).  Drawing at `bh - 4` pushed
                    // every glyph below the badge, which left a solid red plate
                    // with no text for labels that have no pre-rendered PNG
                    // (抽烟/打电话, 人员聚集) — the "检测标签无字" report.
                    ft2_->putText(badge, ld.first, cv::Point(4, 2), 30,
                                  cv::Scalar(255, 255, 255), 1, cv::LINE_AA, false);
                    if (profiling() &&
                        (ld.first != last_badge_label_ ||
                         now_ms() - last_badge_ms_ > 5000)) {
                        last_badge_label_ = ld.first;
                        last_badge_ms_ = now_ms();
                        log_info(cfg_.task_id + " label badge text=" + ld.first +
                                 " size=" + std::to_string(bw) + "x" +
                                 std::to_string(bh));
                    }
                    int x = std::max(0, std::min(ld.second.x - 4, canvas.cols - bw));
                    int y = std::max(0, std::min(ld.second.y - (bh - 4), canvas.rows - bh));
                    badge.copyTo(canvas(cv::Rect(x, y, bw, bh)));
                } catch (const std::exception& e) {
                    log_warn(cfg_.task_id + " label badge: " + e.what());
                }
            }
            if (!font_ok_) {
                cv::putText(canvas, ld.first, ld.second,
                            cv::FONT_HERSHEY_SIMPLEX, 0.55,
                            cv::Scalar(255, 255, 255), 1);
            }
        }
        std::vector<unsigned char> buf;
        {
            std::lock_guard<std::mutex> lk(jpeg_hw_mutex());
            sail::BMImage canvas_bm = sail::Bmcv::mat_to_bm_image(canvas);
            std::string ext = ".jpg";
            bmcv.imencode(ext, canvas_bm, buf);
        }
        {
            // Live previews skip the Pillow HTTP worker (it costs hundreds of
            // ms per frame and blocks the 10fps path). Alarm evidence still
            // gets the full label overlay via alarm_out.
            bool want_label = false;  // CV/FreeType already drew the labels
            if (!want_label) return buf;
            last_label_ms_ = now_ms();
            const char* url_env = std::getenv("AIDETECT_LABEL_URL");
            std::string url = url_env ? url_env : "10.10.206.68:9070";
            size_t colon = url.rfind(':');
            if (colon != std::string::npos) {
                std::string host = url.substr(0, colon);
                std::string port = url.substr(colon + 1);
                Json req;
                req["image"] = base64_encode(buf.data(), buf.size());
                req["out"] = alarm_out;
                req["boxes"] = Json::array();
                for (auto& d : dets) {
                    req["boxes"].push_back({{"x1", d.x1 * sx},
                                            {"y1", d.y1 * sy},
                                            {"label", d.label}});
                }
                auto reply = http_post_raw(host, port, "/draw", req.dump(), 2000);
                log_warn(cfg_.task_id + " label reply=" + std::to_string(reply.size()));
                if (reply.size() > 1024 &&
                    static_cast<unsigned char>(reply[0]) == 0xff &&
                    static_cast<unsigned char>(reply[1]) == 0xd8) {
                    buf.assign(reply.begin(), reply.end());
                }
            }
        }
        if (alarm_render_) {
            std::ofstream df("/tmp/render_final.jpg", std::ios::binary);
            df.write(reinterpret_cast<const char*>(buf.data()), buf.size());
        }
        return buf;    }
    bool update_alarm(ModelRuntime& rt, std::vector<Detection>& dets,
                      sail::BMImage& frame, sail::Bmcv& bmcv) {
        // Filter before alarm state/count/labels so a target outside the ROI
        // neither alarms nor remains in the preview vector.
        dets = filter_roi(dets, rt, frame.width(), frame.height());
        if (dets.empty()) return false;
        int count = 0;
        for (auto& d : dets)
            if (d.target || d.class_id == rt.spec->target_id) ++count;
        int exceed = rt.config.exceed > 0 ? rt.config.exceed : rt.spec->default_exceed;
        bool enough = count >= std::max(1, exceed);
        int64_t now = now_ms();
        std::vector<char> used(rt.targets.size(), 0);
        bool any_confirmed = false;
        auto iou_of = [](const cv::Rect2f& a, const cv::Rect2f& b) {
            float x1 = std::max(a.x, b.x), y1 = std::max(a.y, b.y);
            float x2 = std::min(a.x + a.width, b.x + b.width);
            float y2 = std::min(a.y + a.height, b.y + b.height);
            float iw = std::max(0.f, x2 - x1), ih = std::max(0.f, y2 - y1);
            float inter = iw * ih;
            float uni = a.width * a.height + b.width * b.height - inter;
            return uni > 0 ? inter / uni : 0.f;
        };
        float keep_s = rt.config.keep_sec > 0 ? rt.config.keep_sec : 3.0f;
        for (auto& d : dets) {
            if (!d.target && d.class_id != rt.spec->target_id &&
                d.class_id != rt.spec->target_id2)
                continue;
            float det_keep_s = keep_s;
            if (rt.spec->name == "smoking_call") {
                if (d.class_id == 0) det_keep_s = rt.config.smoking_keep_sec;
                else if (d.class_id == 1 && rt.config.calling_keep_sec > 0)
                    det_keep_s = rt.config.calling_keep_sec;
                else if (d.class_id == 2 && rt.config.playing_keep_sec > 0)
                    det_keep_s = rt.config.playing_keep_sec;
            }
            int64_t keep_ms = static_cast<int64_t>(det_keep_s * 1000.0f);
            cv::Rect2f rect(d.x1, d.y1, std::max(1.f, d.x2 - d.x1), std::max(1.f, d.y2 - d.y1));
            int best = -1;
            float best_iou = 0.15f;
            for (size_t i = 0; i < rt.targets.size(); ++i) {
                if (used[i]) continue;
                float v = iou_of(rect, rt.targets[i].rect);
                if (v > best_iou) { best_iou = v; best = static_cast<int>(i); }
            }
            // Do not fall back to centre distance: a new person standing near
            // an already-confirmed red target inherited its state and turned
            // red immediately, skipping the 5s blue countdown. Only a real
            // IoU match keeps a target's state.
            if (best < 0) {
                ModelRuntime::TargetState st;
                st.rect = rect;
                st.last_seen_ms = now;
                rt.targets.push_back(st);
                used.push_back(1);
                best = static_cast<int>(rt.targets.size() - 1);
            } else {
                used[best] = 1;
            }
            auto& st = rt.targets[best];
            st.last_seen_ms = now;
            st.rect = rect;
            // Production semantics: `最小上报间隔(秒，默认0不重复上报)`.
            // 0 = report once for this target, >0 = repeat after that interval.
            // The old global cooldown alone produced one alarm per frame when a
            // task set cooldownTime=0 (e.g. the V3 reference test).
            // A positive repeat interval is a per-algorithm rate limit across
            // all targets; otherwise noisy detectors (boat_life_jacket emits
            // 70+ boxes) create new target ids every frame and alarm non-stop.
            // The task-level "告警上报间隔(分钟)" is a hard floor; a
            // per-algorithm minimum interval can only make it longer. This
            // stops vest (30s parameter) from reporting 3 times inside the
            // task's configured 3 minutes.
            int64_t task_gap = cfg_.cooldown_seconds * 1000LL;
            int64_t algo_gap = static_cast<int64_t>(rt.config.repeat_sec * 1000.0f);
            // Hard floor: the task-level "告警上报间隔(分钟)" can never make
            // the algorithm's "最小上报间隔(秒)" shorter.  A positive
            // algorithm interval (e.g. 86400) wins for every target, not just
            // the first one; explicit 0 keeps production's "report once per
            // target, then wait for the task interval" semantics.
            int64_t report_gap = std::max(task_gap, algo_gap);
            if (report_gap <= 0) report_gap = task_gap;
            const bool gate_open =
                rt.next_report_ms == 0 || now >= rt.next_report_ms;
            bool alarm_due = false;
            if (rt.config.has_repeat_param && rt.config.repeat_sec <= 0) {
                alarm_due = gate_open && !st.alarmed &&
                            now - rt.last_alarm_ms >= task_gap;
            } else {
                alarm_due = gate_open && now - rt.last_alarm_ms >= report_gap;
            }
            if (!alarm_due && rt.confirmed && now - rt.last_log_ms > 5000) {
                rt.last_log_ms = now;
                log_info(cfg_.task_id + " alarm gate model=" + rt.spec->name +
                         " gap_ms=" + std::to_string(report_gap) +
                         " left_ms=" +
                         std::to_string(std::max<int64_t>(0, rt.next_report_ms - now)));
            }
            if (enough && !st.confirmed) {
                if (st.first_seen_ms == 0) st.first_seen_ms = now;
                if (now - st.first_seen_ms >= keep_ms) {
                    st.confirmed = true;
                    if (alarm_due) {
                        st.alarmed = true;
                        st.last_alarm_ms = now;
                        rt.last_alarm_ms = now;
                        rt.next_report_ms = now + report_gap;
                        last_alarm_ms_ = now;
                        log_info(cfg_.task_id + " alarm gate open model=" +
                                 rt.spec->name + " gap_ms=" +
                                 std::to_string(report_gap));
                        if (!file_source_) save_alarm(rt, dets, count, frame, bmcv);
                    }
                }
            } else if (st.confirmed && alarm_due) {
                st.alarmed = true;
                st.last_alarm_ms = now;
                rt.last_alarm_ms = now;
                rt.next_report_ms = now + report_gap;
                last_alarm_ms_ = now;
                log_info(cfg_.task_id + " alarm gate open model=" + rt.spec->name +
                         " gap_ms=" + std::to_string(report_gap));
                if (!file_source_) save_alarm(rt, dets, count, frame, bmcv);
            }
            d.confirmed = st.confirmed;
            d.pending = !st.confirmed;
            // Inside the report interval keep the red box but hide the alarm
            // label, otherwise the UI looks like it is alarming every frame.
            if (st.confirmed && !alarm_due) d.suppress_label = true;
            if (d.pending && st.first_seen_ms > 0)
                d.countdown_s = std::max(0, static_cast<int>(det_keep_s) -
                                               static_cast<int>((now - st.first_seen_ms) / 1000));
            if (st.confirmed) any_confirmed = true;
        }
        // A target that disappears for longer than the TTL loses its confirmed
        // state; a later re-appearance starts a new countdown.  The TTL used to
        // be 1s, which erased intermittent classifiers (smoking/calling flips
        // class every round) before they could reach their 5s keep time, so the
        // 抽烟 label never appeared.  Preview ghosting is handled by the
        // tracker's 600ms person-anchor grace, not by this alarm TTL.
        for (size_t i = 0; i < rt.targets.size();) {
            if (now - rt.targets[i].last_seen_ms > target_ttl_ms_) {
                rt.targets.erase(rt.targets.begin() + static_cast<long>(i));
                continue;
            }
            ++i;
        }
        rt.confirmed = any_confirmed;
        return any_confirmed;
    }
    // Production's MergeInstance draws `AlgTaskSession` as OSD in the
    // top-right corner of every tile that belongs to a task.  Keep the same
    // black plate + white text, including on reconnect/status tiles.
    cv::Mat task_bar_mat(int target_w, int target_h) {
        if (!font_ok_ || !ft2_ || cfg_.task_name.empty() || target_w < 80)
            return {};
        const int font = target_h >= 720 ? 28 : 16;
        const int pad_x = target_h >= 720 ? 14 : 8;
        const int pad_y = target_h >= 720 ? 8 : 4;
        const int max_w = std::max(120, static_cast<int>(target_w * 0.62));
        int base = 0;
        std::string label = cfg_.task_name;
        cv::Size ts = ft2_->getTextSize(label, font, 1, &base);
        if (ts.width + 2 * pad_x > max_w) {
            // Trim whole UTF-8 code points and keep an ASCII ellipsis.
            std::string cut = label;
            while (!cut.empty()) {
                size_t p = cut.size() - 1;
                while (p > 0 && (static_cast<unsigned char>(cut[p]) & 0xC0) == 0x80)
                    --p;
                cut.erase(p);
                ts = ft2_->getTextSize(cut + "...", font, 1, &base);
                if (ts.width + 2 * pad_x <= max_w) {
                    label = cut + "...";
                    break;
                }
            }
        }
        int bw = std::min(target_w, ts.width + 2 * pad_x);
        int bh = std::min(target_h, ts.height + 2 * pad_y);
        if (bw < 8 || bh < 8) return {};
        cv::Mat bar(bh, bw, CV_8UC3, cv::Scalar(0, 0, 0));
        // OpenCV FreeType2 uses the top-left corner as the text origin here;
        // using ts.height + pad_y pushed every glyph below the plate and left
        // a solid black bar.
        ft2_->putText(bar, label, cv::Point(pad_x, std::min(bh - 1, pad_y)),
                      font, cv::Scalar(255, 255, 255), 1, cv::LINE_AA, false);
        return bar;
    }
    void draw_task_bar(cv::Mat& canvas) {
        cv::Mat bar = task_bar_mat(canvas.cols, canvas.rows);
        if (bar.empty()) return;
        int x = std::max(0, canvas.cols - bar.cols);
        bar.copyTo(canvas(cv::Rect(x, 0, bar.cols, bar.rows)));
    }
    std::shared_ptr<sail::BMImage> task_bar_bm(int target_w, int target_h) {
        if (!font_ok_ || !ft2_ || cfg_.task_name.empty()) return nullptr;
        std::string key = std::to_string(target_w) + "x" + std::to_string(target_h);
        {
            std::lock_guard<std::mutex> lk(bar_bm_mu_);
            auto it = bar_bm_cache_.find(key);
            if (it != bar_bm_cache_.end()) return it->second;
        }
        cv::Mat bar = task_bar_mat(target_w, target_h);
        if (bar.empty()) return nullptr;
        auto bm = std::make_shared<sail::BMImage>(sail::Bmcv::mat_to_bm_image(bar));
        std::lock_guard<std::mutex> lk(bar_bm_mu_);
        bar_bm_cache_[key] = bm;
        return bm;
    }
    void set_stream_status(const std::string& text) {
        std::lock_guard<std::mutex> lk(status_text_mu_);
        stream_status_ = text;
    }
    void publish_status(const std::string& text, bool force = false,
                        bool remember = true) {
        if (stop_) return;
        if (remember) set_stream_status(text);
        int64_t now = now_ms();
        if (!force && now - last_status_ms_ < 400) return;
        last_status_ms_ = now;
        // A single-channel viewer scales the JPEG to full width; 640x360 made
        // the reconnect text very blurry.  Render 720p only while somebody is
        // watching that one channel, otherwise keep the cheap 640x360 tile.
        const bool hd = want_single_ && want_single_();
        const int sw = hd ? 1280 : 640;
        const int sh = hd ? 720 : 360;
        const double ui_scale = sw / 640.0;
        cv::Mat canvas(sh, sw, CV_8UC3, cv::Scalar(0, 0, 0));
        std::string line1 = "监控视频流状态";
        time_t now_sec = time(nullptr);
        struct tm tmv;
        localtime_r(&now_sec, &tmv);
        char clock[32];
        std::strftime(clock, sizeof(clock), "%H:%M:%S", &tmv);
        std::string clock_line = clock;
        std::lock_guard<std::mutex> font_lock(font_mutex());
        std::vector<std::string> lines;
        std::string rest = text;
        const size_t max_bytes = 24;
        while (!rest.empty() && lines.size() < 3) {
            size_t n = std::min(max_bytes, rest.size());
            while (n < rest.size() &&
                   (static_cast<unsigned char>(rest[n]) & 0xC0) == 0x80)
                --n;
            if (n == 0) n = std::min(max_bytes, rest.size());
            lines.push_back(rest.substr(0, n));
            rest.erase(0, n);
        }
        if (!font_ok_ || !ft2_) {
            int baseline = 0;
            auto sz = cv::getTextSize("STREAM STATUS", cv::FONT_HERSHEY_SIMPLEX,
                                      1.0, 2, &baseline);
            cv::putText(canvas, "STREAM STATUS",
                        cv::Point((sw - sz.width) / 2, sh / 2 - 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
        } else {
            try {
                const int title_font = static_cast<int>(30 * ui_scale);
                const int line_font = static_cast<int>(22 * ui_scale);
                int base = 0;
                auto ts = ft2_->getTextSize(line1, title_font, 1, &base);
                int y = sh / 2 - static_cast<int>(lines.size()) * line_font / 2 - 12;
                ft2_->putText(canvas, line1,
                              cv::Point((sw - ts.width) / 2, y), title_font,
                              cv::Scalar(255, 255, 255), 1, cv::LINE_AA, false);
                for (size_t i = 0; i < lines.size(); ++i) {
                    auto ls = ft2_->getTextSize(lines[i], line_font, 1, &base);
                    ft2_->putText(canvas, lines[i],
                                  cv::Point((sw - ls.width) / 2,
                                            y + title_font + 14 +
                                                static_cast<int>(i) * (line_font + 8)),
                                  line_font, cv::Scalar(255, 255, 255), 1, cv::LINE_AA,
                                  false);
                }
                const int clock_font = static_cast<int>(20 * ui_scale);
                auto cs = ft2_->getTextSize(clock_line, clock_font, 1, &base);
                ft2_->putText(canvas, clock_line,
                              cv::Point((sw - cs.width) / 2,
                                        static_cast<int>(30 * ui_scale)),
                              clock_font, cv::Scalar(200, 200, 200), 1,
                              cv::LINE_AA, false);
                // Production renders the task session/编号 as OSD on every
                // live or reconnecting tile; the no-task placeholder is
                // generated by MergeInstance and deliberately has no bar.
                draw_task_bar(canvas);
            } catch (...) {
                font_ok_ = false;
            }
        }
        std::vector<unsigned char> buf;
        {
            std::lock_guard<std::mutex> lk(jpeg_hw_mutex());
            cv::imencode(".jpg", canvas, buf);
        }
        if (buf.empty()) return;        {
            std::lock_guard<std::mutex> lk(jpeg_mutex_);
            latest_jpeg_ = std::make_shared<std::vector<unsigned char>>(buf);
        }
        if (single_cb_) single_cb_(cfg_.task_id, buf);
        if (tile_cb_) tile_cb_(cfg_.task_id, group_, slot_, buf);
    }
    void infer_loop() {
        sail::Handle handle(0);
        sail::Bmcv bmcv(handle);
        while (!stop_) {
            apply_pending_config();
            InferJob job;
            {
                std::unique_lock<std::mutex> lk(infer_mu_);
                infer_cv_.wait_for(lk, std::chrono::milliseconds(200),
                                   [this] { return stop_ || !infer_q_.empty(); });
                if (stop_) break;
                if (infer_q_.empty()) continue;
                job = std::move(infer_q_.front());
                infer_q_.pop_front();
            }
            apply_pending_config();
            try {
                if (job.img) detect(*job.img, handle, bmcv, job.ts);
            } catch (const std::exception& e) {
                log_error(cfg_.task_id + " async detect: " + e.what());
            }
        }
    }
    void apply_pending_config() {
        TaskConfig tc;
        {
            std::lock_guard<std::mutex> lk(pending_cfg_mu_);
            if (!has_pending_cfg_) return;
            tc = pending_cfg_;
            has_pending_cfg_ = false;
        }
        std::vector<std::unique_ptr<ModelRuntime>> fresh;
        for (auto& m : tc.models) {
            const ModelSpec* spec = scheduler_.spec(m.name);
            if (!spec) {
                log_warn(tc.task_id + " hot config unsupported model: " + m.name);
                continue;
            }
            auto rt = std::make_unique<ModelRuntime>();
            rt->config = m;
            rt->spec = spec;
            rt->display_label = display_label_for(*rt);
            rt->min_interval_ms = algorithm_interval_ms(rt->spec);
            max_alg_interval_ms_ = std::max(max_alg_interval_ms_, rt->min_interval_ms);
            person_grace_ms_ = std::max<int64_t>(600, max_alg_interval_ms_ * 3 / 2);
            fresh.push_back(std::move(rt));
        }
        bool need_people = false;
        for (auto& rt : fresh) {
            if (!rt->spec) continue;
            if (rt->spec->type == ModelType::AttrClassifier ||
                rt->spec->type == ModelType::YoloV10Crop ||
                rt->spec->type == ModelType::BoatLifeJacket ||
                rt->spec->type == ModelType::YoloV5Crop)
                need_people = true;
        }
        auto add_base = [&](const std::string& name) {
            for (auto& rt : fresh)
                if (rt->spec && rt->spec->name == name) return;
            const ModelSpec* ps = scheduler_.spec(name);
            if (!ps) return;
            auto rt = std::make_unique<ModelRuntime>();
            rt->config.name = name;
            rt->config.conf = 0.3f;
            rt->config.nms = 0.7f;
            rt->config.exceed = 999;
            rt->spec = ps;
            rt->display_label = ps->report_content;
            rt->base_only = true;
            fresh.push_back(std::move(rt));
        };
        bool people_deep = false;
        for (auto& rt : fresh)
            if (rt->spec && rt->config.people_deep) people_deep = true;
        if (need_people) add_base(people_deep ? "people_big" : "people");
        for (auto& rt : fresh) {
            if (!rt->spec ||
                (rt->spec->name != "people" && rt->spec->name != "people_big"))
                continue;
            float pc = rt->config.people_conf;
            bool deep = rt->config.people_deep;
            for (auto& o : fresh) {
                if (o.get() == rt.get() || !o->spec) continue;
                pc = std::max(pc, o->config.people_conf);
                deep = deep || o->config.people_deep;
            }
            rt->config.conf = pc;
            log_info(tc.task_id + " people base conf=" + std::to_string(pc) +
                     " deep=" + (deep ? "1" : "0"));
        }
        std::vector<std::vector<std::pair<float, float>>> new_rois;
        for (auto& rt : fresh) {
            for (auto& poly : rt->config.polygons) {
                if (poly.size() < 3) continue;
                bool dup = false;
                for (auto& have : new_rois) {
                    if (have.size() != poly.size()) continue;
                    float d = 0.0f;
                    for (size_t i = 0; i < poly.size(); ++i) {
                        d = std::max(d, std::abs(have[i].first - poly[i].first));
                        d = std::max(d, std::abs(have[i].second - poly[i].second));
                    }
                    if (d < 0.002f) { dup = true; break; }
                }
                if (!dup) new_rois.push_back(poly);
            }
        }
        const size_t nmodels = fresh.size();
        const size_t nrois = new_rois.size();
        {
            std::lock_guard<std::mutex> lk(models_mu_);
            models_.swap(fresh);
        }
        {
            std::lock_guard<std::mutex> lk(roi_mu_);
            roi_polygons_.swap(new_rois);
        }
        cfg_.models = tc.models;
        cfg_.cooldown_seconds = tc.cooldown_seconds;
        cfg_.video_duration = tc.video_duration;
        {
            std::lock_guard<std::mutex> lk(det_mutex_);
            dets_.clear();
            dets_persons_.clear();
            dets_history_.clear();
            dets_ms_ = 0;
        }
        for (auto& rt : fresh)
            if (rt->spec) ensure_label_plate(rt->spec->report_content);
        log_info(tc.task_id + " hot config applied models=" +
                 std::to_string(nmodels) + " rois=" +
                 std::to_string(nrois));
    }
    void process_frame(sail::BMImage& frame, sail::Handle& handle,
                       sail::Bmcv& bmcv, int& frame_id) {
        const int64_t frame_ts = now_ms();
        last_frame_ms_ = frame_ts;
        queued_frame_ms_ = frame_ts;
        current_frame_ms_ = frame_ts;
        if (need_recover_status_.exchange(false))
            publish_status("监控视频流已恢复");
        if (!ever_frame_.exchange(true)) set_stream_error("");
            last_progress_ms_ = now_ms();
        if (debug_alarm_.exchange(false) && !models_.empty()) {
            std::vector<Detection> dd;
            {
                std::lock_guard<std::mutex> lk(det_mutex_);
                dd = dets_;
            }
            // Under AIDETECT_DEBUG the explicit /debug/trigger_alarm call is
            // enough; requiring a second env var made live video verification
            // impossible on the deployed container.
            if (dd.empty() && std::getenv("AIDETECT_DEBUG")) {
                // Verification aid: /debug/trigger_alarm must work on a scene
                // where the model currently has no detections.
                Detection d;
                d.x1 = frame.width() * 0.35f;
                d.y1 = frame.height() * 0.30f;
                d.x2 = frame.width() * 0.55f;
                d.y2 = frame.height() * 0.75f;
                d.score = 0.90f;
                d.target = true;
                d.class_id = 1;
                if (!models_[0]->spec->class_names.empty())
                    d.label = models_[0]->spec->class_names.back();
                dd.push_back(d);
                log_info(cfg_.task_id + " debug forced detection");
            }
            if (!dd.empty()) {
                try {
                    if (!file_source_)
                        save_alarm(*models_[0], dd, static_cast<int>(dd.size()), frame, bmcv);
                } catch (const std::exception& e) {
                    log_error(cfg_.task_id + " debug alarm: " + e.what());
                }
            } else {
                log_info(cfg_.task_id + " debug alarm ignored (no detections yet)");
            }
        }
        ++frame_id;
        int64_t now = now_ms();
        // Preview is generated on demand from the same decoded frame: no second
        // ffmpeg decode, no hard 1920x1080 upscale. The vendor does the same via
        // DecodeInstance::requireScaled()/revokeScaled() and a per-task JPEG
        // encoder that only runs while a browser watches the channel.
        maybe_preview(frame, handle, bmcv, now);
        // 4K sources pay ~4x the VPP cost per full-frame resize and per crop.
        // Slowing only their detection cadence keeps the shared JPU/VPU free
        // for the preview path; boxes between detections are advanced by the
        // preview tracker on every frame, so 2 Hz is still smooth.
        int64_t detect_iv = std::max(50, detect_min_interval_ms_);
        if (static_cast<int64_t>(frame.width()) * frame.height() > 4000000)
            detect_iv = detect_iv * 3 / 2;
        // Adaptive skip: when the shared inference pipeline is deeper than 2x
        // its admission limit, slow this task down (production skips frames
        // instead of queueing them).  Capped at 3x so detection never dies.
        {
            const double load = scheduler_.load();
            if (load > 2.0) {
                const double f = std::min(3.0, load / 2.0);
                detect_iv = static_cast<int64_t>(detect_iv * f);
            }
        }
        if (frame_id == 1 || now - last_detect_ms_ >= detect_iv) {
            last_detect_ms_ = now;
            if (std::getenv("AIDETECT_NO_INFER")) {
                // Diagnostic mode: measure pure decode/render throughput.
            } else {
                // Inference always runs on the dedicated infer thread, even in
                // single-decoder mode. Running detect() inline used to stall the
                // decode loop for hundreds of ms under 30 tasks, which capped
                // the live preview at 2~5fps even though encoding was fast.
                sail::BMImage copy(handle, frame.height(), frame.width(),
                                   frame.format(), DATA_TYPE_EXT_1N_BYTE);
                if (bmcv.image_copy_to(frame, copy, 0, 0) == 0) {
                    InferJob job;
                    job.img = std::make_shared<sail::BMImage>(std::move(copy));
                    job.ts = frame_ts;
                    std::lock_guard<std::mutex> lk(infer_mu_);
                    // Keep only the newest frame: a two-deep FIFO made the
                    // detector work on an older image while preview showed the
                    // current one, which is the other half of the box lag.
                    while (!infer_q_.empty()) infer_q_.pop_front();
                    infer_q_.push_back(std::move(job));
                    infer_cv_.notify_one();
                }
            }
        }
    }
    // 10fps content gate. For sources at or below 10fps every decoded frame is
    // forwarded (no duplicated, no dropped frames); above 10fps a token bucket
    // keeps the average at exactly 10fps while always emitting fresh frames.
    // Merge tiles use a second bucket at `tile_interval_ms_`.
    bool preview_due(int64_t now, bool tile) {
        int64_t& last = tile ? tile_last_token_ms_ : last_token_ms_;
        double& tokens = tile ? tile_tokens_ : tokens_;
        int64_t interval = tile ? tile_interval_ms_ : preview_interval_ms_;
        double target_fps = 1000.0 / static_cast<double>(std::max<int64_t>(1, interval));
        if (last == 0) {
            last = now;
            tokens = 1.0;
        } else {
            double dt = static_cast<double>(now - last);
            tokens = std::min(2.0, tokens + dt * target_fps / 1000.0);
            last = now;
        }
        if (tokens >= 1.0) {
            tokens -= 1.0;
            return true;
        }
        return false;
    }
    struct Size2 {
        int w = 0;
        int h = 0;
    };
    // Resolution rule: only downscale, never upscale. Anything above 1080p is
    // fitted inside 1920x1080 (aspect kept); 1080p and below stays untouched.
    Size2 preview_target(int w, int h) {
        if (preview_w_ > 0 && preview_h_ > 0) return {preview_w_, preview_h_};
        if (w <= 0 || h <= 0) return {1920, 1080};
        if (w <= 1920 && h <= 1080) return {w, h};
        // bmcv.vpp_resize is only pixel-correct for an integer divisor, so
        // pick the smallest k that brings the frame under 1920x1080 while
        // keeping both dimensions divisible (2560x1440 -> 1280x720,
        // 3840x2160 -> 1920x1080, 4096x2160 -> 1024x540, 3200x1800 ->
        // 1600x900).  This keeps the whole preview path on VPP/JPU instead
        // of falling back to a CPU resize.
        for (int k = 2; k <= 8; ++k) {
            if (w % k != 0 || h % k != 0) continue;
            if (w / k > 1920 || h / k > 1080) continue;
            return {w / k, h / k};
        }
        return {w, h};
    }
    // Preview encoding used to run inline on the decode thread.  With nine
    // merge tiles this made the decode loop wait for the serialized JPU mutex;
    // the decoder kept delivering older packets and the merged wall clock ran
    // slower and slower.  Decode now only copies the latest frame when the
    // preview worker is idle, and the worker drops stale frames instead of
    // queueing them, so merge output stays wall-clock realtime.
    // One preview track = one detection target that is continuously bound to
    // the frames it was seen on.  Production stores boxes per frame in
    // `FramePackage::setBoxes(frameId, boxes)` and keeps target identity via
    // `board::track::IoUTracker`; the preview never re-applies a box from an
    // older frame to a newer one.  We mirror that contract:
    //   - the box is advanced on every preview frame (LK + motion model),
    //   - a new detection is associated against the track's *historical* box
    //     at the detection frame's timestamp (not against "now"),
    //   - only the position/size delta is applied to the current box, so the
    //     box never snaps back to where the person was one second ago,
    //   - a track whose target is absent from the newest detection snapshot,
    //     or whose centre leaves the task ROI, is removed immediately.
    struct PreviewTrack {
        Detection proto;                 // score/state/label from last detection
        cv::Rect2f box;                  // current box, in tracking-frame pixels
        std::vector<cv::Point2f> pts;    // LK points on the current frame
        std::deque<std::pair<int64_t, cv::Rect2f>> hist;  // (ts, box)
        int64_t last_match_ms = 0;
        int64_t last_det_ts = 0;
        int64_t last_person_ms = 0;  // last snapshot that still had a person anchor
        float vx = 0, vy = 0;            // tracking px / second
        bool dead = false;               // person anchor disappeared
    };
    std::vector<Detection> track_preview_boxes(sail::BMImage& frame,
                                               sail::BMImage& scratch,
                                               sail::Bmcv& bmcv,
                                               std::vector<Detection> dets,
                                               std::vector<Detection> persons,
                                               int64_t dets_ts,
                                               int64_t now) {
        if (frame.width() <= 0 || frame.height() <= 0) return dets;
        // 960 px is enough for a 20x100 px person at the far end of a 4K
        // camera.  Resize/convert on the VPU first: converting a 2560x1440
        // frame to CPU BGR on the decode thread at 10 fps was itself eating
        // the preview budget and dropped single-channel output to ~4 fps.
        // Same VPP limitation as the preview scaler: only integral factors
        // are correct, so halve until <=1280 instead of resizing to 960
        // (2560 -> 960 is x0.375 and came back with corrupted rows).
        int tw = frame.width();
        while (tw > 1280 && tw % 2 == 0) tw /= 2;
        int th = static_cast<int>(std::lround(
            frame.height() * static_cast<double>(tw) / frame.width()));
        tw &= ~1;
        th &= ~1;
        if (tw < 2) tw = 2;
        if (th < 2) th = 2;
        const double sx = static_cast<double>(tw) / frame.width();
        const double sy = static_cast<double>(th) / frame.height();
        cv::Mat mat;
        {
            if (bmcv.vpp_resize(frame, scratch, tw, th,
                                bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0)
                return {};
            sail::BMImage bgr = bmcv.vpp_convert_format(scratch, FORMAT_BGR_PACKED);
            cv::Mat view;
            sail::Bmcv::bm_image_to_mat(bgr, view);
            mat = view.clone();
        }
        if (mat.empty()) return dets;
        cv::Mat gray;
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
        const float fw = static_cast<float>(gray.cols);
        const float fh = static_cast<float>(gray.rows);
        auto median = [](std::vector<float> v) -> float {
            if (v.empty()) return 0.0f;
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            return v[v.size() / 2];
        };
        auto make_points = [&](const cv::Rect2f& b) {
            std::vector<cv::Point2f> pts;
            // Sample the inner torso/legs, not the box border.  A detection
            // box almost always includes some floor/ROI line at its edges; LK
            // on those static pixels dominates the median and makes the box
            // lag behind a walking person.
            for (int gy = 0; gy < 5; ++gy) {
                for (int gx = 0; gx < 5; ++gx) {
                    pts.emplace_back(b.x + b.width * (0.20f + 0.60f * gx / 4.0f),
                                     b.y + b.height * (0.22f + 0.66f * gy / 4.0f));
                }
            }
            return pts;
        };
        auto clamp_box = [&](cv::Rect2f& b) {
            b.width = std::max(2.0f, b.width);
            b.height = std::max(2.0f, b.height);
            if (b.x < 0) b.x = 0;
            if (b.y < 0) b.y = 0;
            if (b.x + b.width > fw) b.width = std::max(2.0f, fw - b.x);
            if (b.y + b.height > fh) b.height = std::max(2.0f, fh - b.y);
        };
        std::vector<std::vector<std::pair<float, float>>> rois;
        {
            std::lock_guard<std::mutex> lk(roi_mu_);
            rois = roi_polygons_;
        }
        auto center_in_roi = [&](const cv::Rect2f& b) {
            if (rois.empty()) return true;
            float cx = (b.x + b.width * 0.5f) / fw;
            float cy = (b.y + b.height * 0.5f) / fh;
            for (auto& poly : rois)
                if (poly.size() >= 3 && point_in_poly(poly, cx, cy)) return true;
            return false;
        };
        auto hist_at = [](const PreviewTrack& t, int64_t ts,
                          cv::Rect2f& out) -> bool {
            int64_t best = INT64_MAX;
            for (auto& h : t.hist) {
                int64_t d = h.first > ts ? h.first - ts : ts - h.first;
                if (d < best) {
                    best = d;
                    out = h.second;
                }
            }
            return best <= 350;
        };
        const int64_t dt_ms =
            preview_prev_ts_ > 0 ? std::max<int64_t>(1, now - preview_prev_ts_) : 0;
        // A viewer can close the page and reopen minutes later.  Flowing
        // between those two unrelated frames would put every box on garbage;
        // start fresh instead.
        if (preview_prev_ts_ > 0 && now - preview_prev_ts_ > 1500) {
            preview_tracks_.clear();
            preview_prev_gray_.release();
        }
        // A confirmed label may miss one classifier pass; keep it while the
        // tracked box stays in the ROI, but never longer than two detection
        // intervals.  The ROI test below removes a person who actually left on
        // the frame they cross the line, not after this TTL.
        int64_t stale_limit = std::max<int64_t>(1500, preview_det_interval_ms_ * 2);
        {
            std::vector<PreviewTrack> live;
            live.reserve(preview_tracks_.size());
            for (auto& t : preview_tracks_) {
                if (now - t.last_match_ms > stale_limit) continue;
                live.push_back(std::move(t));
            }
            preview_tracks_ = std::move(live);
        }
        const bool prev_ok =
            !preview_prev_gray_.empty() && preview_prev_gray_.size() == gray.size();
        std::vector<PreviewTrack> next;
        next.reserve(preview_tracks_.size());
        for (auto& t : preview_tracks_) {
            float dx = 0.0f, dy = 0.0f, scale = 1.0f;
            bool flowed = false;
            if (prev_ok && t.pts.size() >= 6) {
                std::vector<cv::Point2f> np, back;
                std::vector<uchar> st, bst;
                std::vector<float> er, ber;
                cv::calcOpticalFlowPyrLK(
                    preview_prev_gray_, gray, t.pts, np, st, er, cv::Size(31, 31), 4,
                    cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                                     20, 0.03));
                cv::calcOpticalFlowPyrLK(
                    gray, preview_prev_gray_, np, back, bst, ber, cv::Size(31, 31), 4,
                    cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                                     20, 0.03));
                std::vector<float> dxs, dys;
                std::vector<cv::Point2f> oldp, newp;
                for (size_t i = 0; i < np.size(); ++i) {
                    if (!st[i] || !bst[i] || er[i] > 40.0f || ber[i] > 40.0f) continue;
                    if (cv::norm(back[i] - t.pts[i]) > 1.5f) continue;
                    dxs.push_back(np[i].x - t.pts[i].x);
                    dys.push_back(np[i].y - t.pts[i].y);
                    oldp.push_back(t.pts[i]);
                    newp.push_back(np[i]);
                }
                // A translation estimated from at least 6 stable points is
                // accepted; the median rejects points on the moving floor or
                // on a passing background object.
                if (oldp.size() >= 6) {
                    dx = median(dxs);
                    dy = median(dys);
                    float span = std::max(1.0f, std::max(t.box.width, t.box.height));
                    if (std::sqrt(dx * dx + dy * dy) <= span * 0.6f) {
                        cv::Point2f c0(0, 0), c1(0, 0);
                        for (auto& p : oldp) c0 += p;
                        for (auto& p : newp) c1 += p;
                        c0 *= 1.0f / oldp.size();
                        c1 *= 1.0f / newp.size();
                        std::vector<float> r0, r1;
                        for (auto& p : oldp) r0.push_back(cv::norm(p - c0));
                        for (auto& p : newp) r1.push_back(cv::norm(p - c1));
                        float m0 = median(r0), m1 = median(r1);
                        if (m0 > 1.0f && m1 > 1.0f)
                            scale = std::max(0.90f, std::min(1.10f, m1 / m0));
                        flowed = true;
                    }
                }
            }
            if (!flowed && prev_ok && t.box.width >= 8 && t.box.height >= 12) {
                // LK can lose a dark, texture-poor torso for a frame.  A
                // template match of the inner box is a cheap second opinion
                // and still finds the person after a large sideways step.
                cv::Rect tmpl_rect(
                    static_cast<int>(t.box.x + t.box.width * 0.15f),
                    static_cast<int>(t.box.y + t.box.height * 0.15f),
                    std::max(4, static_cast<int>(t.box.width * 0.70f)),
                    std::max(6, static_cast<int>(t.box.height * 0.70f)));
                tmpl_rect &= cv::Rect(0, 0, preview_prev_gray_.cols,
                                      preview_prev_gray_.rows);
                int mx = std::max(8, static_cast<int>(t.box.width * 0.85f));
                int my = std::max(8, static_cast<int>(t.box.height * 0.85f));
                cv::Rect search(
                    std::max(0, tmpl_rect.x - mx),
                    std::max(0, tmpl_rect.y - my),
                    std::min(gray.cols, tmpl_rect.x + tmpl_rect.width + mx) -
                        std::max(0, tmpl_rect.x - mx),
                    std::min(gray.rows, tmpl_rect.y + tmpl_rect.height + my) -
                        std::max(0, tmpl_rect.y - my));
                if (tmpl_rect.width >= 6 && tmpl_rect.height >= 8 &&
                    search.width >= tmpl_rect.width + 2 &&
                    search.height >= tmpl_rect.height + 2) {
                    cv::Mat res;
                    cv::matchTemplate(gray(search), preview_prev_gray_(tmpl_rect),
                                      res, cv::TM_CCOEFF_NORMED);
                    double best = 0;
                    cv::Point best_pt;
                    cv::minMaxLoc(res, nullptr, &best, nullptr, &best_pt);
                    if (best >= 0.45) {
                        dx = static_cast<float>(search.x + best_pt.x - tmpl_rect.x);
                        dy = static_cast<float>(search.y + best_pt.y - tmpl_rect.y);
                        float span = std::max(1.0f, std::max(t.box.width, t.box.height));
                        if (std::sqrt(dx * dx + dy * dy) <= span * 0.9f) {
                            flowed = true;
                            scale = 1.0f;
                            t.vx = dx * 1000.0f / std::max<int64_t>(1, dt_ms);
                            t.vy = dy * 1000.0f / std::max<int64_t>(1, dt_ms);
                        }
                    }
                }
            }
            if (flowed) {
                cv::Point2f c(t.box.x + t.box.width * 0.5f,
                              t.box.y + t.box.height * 0.5f);
                c.x += dx;
                c.y += dy;
                float nw = std::max(2.0f, t.box.width * scale);
                float nh = std::max(2.0f, t.box.height * scale);
                t.box = cv::Rect2f(c.x - nw * 0.5f, c.y - nh * 0.5f, nw, nh);
                if (dt_ms > 0) {
                    t.vx = 0.5f * t.vx + 0.5f * (dx * 1000.0f / dt_ms);
                    t.vy = 0.5f * t.vy + 0.5f * (dy * 1000.0f / dt_ms);
                    t.vx = std::max(-2000.0f, std::min(2000.0f, t.vx));
                    t.vy = std::max(-2000.0f, std::min(2000.0f, t.vy));
                }
            } else if (dt_ms > 0) {
                // Short flow dropouts fall back to the last measured velocity;
                // this is still anchored to the previous frame, unlike the old
                // code which re-applied an old detection box.
                t.box.x += t.vx * dt_ms / 1000.0f;
                t.box.y += t.vy * dt_ms / 1000.0f;
            }
            clamp_box(t.box);
            // Frame-level ROI binding: as soon as the tracked person crosses
            // the green line the track is gone on that same preview frame.
            if (!center_in_roi(t.box)) continue;
            t.pts = make_points(t.box);
            t.hist.emplace_back(now, t.box);
            while (!t.hist.empty() && now - t.hist.front().first > 3000)
                t.hist.pop_front();
            next.push_back(std::move(t));
        }
        preview_tracks_ = std::move(next);
        if (dets_ts > 0 && dets_ts != preview_last_det_ts_) {
            if (preview_last_det_ts_ > 0) {
                int64_t d = dets_ts - preview_last_det_ts_;
                preview_det_interval_ms_ =
                    preview_det_interval_ms_ > 0
                        ? static_cast<int64_t>(0.7 * preview_det_interval_ms_ + 0.3 * d)
                        : d;
            }
            std::vector<char> used(preview_tracks_.size(), 0);
            std::vector<cv::Rect2f> det_boxes(dets.size());
            for (size_t k = 0; k < dets.size(); ++k) {
                auto& d = dets[k];
                det_boxes[k] = cv::Rect2f(d.x1 * sx, d.y1 * sy,
                                          (d.x2 - d.x1) * sx, (d.y2 - d.y1) * sy);
            }
            std::vector<cv::Rect2f> person_boxes;
            person_boxes.reserve(persons.size());
            for (auto& p : persons) {
                person_boxes.emplace_back(p.x1 * sx, p.y1 * sy,
                                          (p.x2 - p.x1) * sx, (p.y2 - p.y1) * sy);
            }
            for (size_t k = 0; k < dets.size(); ++k) {
                auto& d = dets[k];
                cv::Rect2f db = det_boxes[k];
                if (db.width < 2 || db.height < 2) continue;
                int best = -1;
                float best_score = -1e9f;
                cv::Rect2f best_ref;
                for (size_t i = 0; i < preview_tracks_.size(); ++i) {
                    if (used[i]) continue;
                    auto& t = preview_tracks_[i];
                    // Keep the association on the label, not class_id: the
                    // behaviour model flips between 吸烟/打电话/玩手机 on the
                    // same person, so class_id changes every round and a
                    // class-based match created a new track each time (the
                    // smoking box never survived long enough to show).
                    if (t.proto.label != d.label) continue;
                    cv::Rect2f ref = t.box;
                    hist_at(t, dets_ts, ref);
                    float ix1 = std::max(db.x, ref.x);
                    float iy1 = std::max(db.y, ref.y);
                    float ix2 = std::min(db.x + db.width, ref.x + ref.width);
                    float iy2 = std::min(db.y + db.height, ref.y + ref.height);
                    float inter = std::max(0.0f, ix2 - ix1) * std::max(0.0f, iy2 - iy1);
                    float uni = db.width * db.height + ref.width * ref.height - inter;
                    float iou = uni > 0 ? inter / uni : 0.0f;
                    float dcx = (db.x + db.width * 0.5f) - (ref.x + ref.width * 0.5f);
                    float dcy = (db.y + db.height * 0.5f) - (ref.y + ref.height * 0.5f);
                    float diag = std::sqrt(ref.width * ref.width + ref.height * ref.height);
                    float cdist = diag > 1.0f ? std::sqrt(dcx * dcx + dcy * dcy) / diag
                                              : 0.0f;
                    // IoU is the primary gate.  A centre-distance fallback
                    // rescues a fast walker whose box drifted slightly during
                    // a long inference delay, but requires the same label.
                    if (iou < 0.20f && cdist > 0.30f) continue;
                    float score = iou - 0.05f * cdist;
                    if (score > best_score) {
                        best_score = score;
                        best = static_cast<int>(i);
                        best_ref = ref;
                    }
                }
                if (best >= 0) {
                    auto& t = preview_tracks_[best];
                    cv::Point2f c(t.box.x + t.box.width * 0.5f,
                                  t.box.y + t.box.height * 0.5f);
                    // Delta between the detection frame and the track's pose
                    // on that same frame.  Apply that delta to the *current*
                    // pose: the box moves forward with the person instead of
                    // snapping back to the detection frame.
                    c.x += (db.x + db.width * 0.5f) -
                           (best_ref.x + best_ref.width * 0.5f);
                    c.y += (db.y + db.height * 0.5f) -
                           (best_ref.y + best_ref.height * 0.5f);
                    // Correct size by the ratio between the detection frame
                    // and the track pose on that same frame, not by the old
                    // absolute size: the person may have walked closer during
                    // the inference delay.
                    float area_ref = std::max(1.0f, best_ref.width * best_ref.height);
                    float area_det = std::max(1.0f, db.width * db.height);
                    float rs = std::sqrt(area_det / area_ref);
                    rs = std::max(0.85f, std::min(1.18f, 0.5f * (1.0f + rs)));
                    float nw = std::max(2.0f, t.box.width * rs);
                    float nh = std::max(2.0f, t.box.height * rs);
                    t.box = cv::Rect2f(c.x - nw * 0.5f, c.y - nh * 0.5f, nw, nh);
                    clamp_box(t.box);
                    t.proto = d;
                    t.last_match_ms = now;
                    t.last_det_ts = dets_ts;
                    t.pts = make_points(t.box);
                    t.hist.emplace_back(dets_ts, db);
                    used[best] = 1;
                } else {
                    // New target.  The detection frame is older than `now`;
                    // shift the box by the median motion of already tracked
                    // targets over the same interval so it does not appear at
                    // a stale position.
                    cv::Point2f shift(0, 0);
                    {
                        std::vector<float> sxv, syv;
                        for (auto& t : preview_tracks_) {
                            cv::Rect2f ref;
                            if (!hist_at(t, dets_ts, ref)) continue;
                            sxv.push_back((t.box.x + t.box.width * 0.5f) -
                                          (ref.x + ref.width * 0.5f));
                            syv.push_back((t.box.y + t.box.height * 0.5f) -
                                          (ref.y + ref.height * 0.5f));
                        }
                        if (!sxv.empty()) {
                            shift.x = median(sxv);
                            shift.y = median(syv);
                        }
                    }
                    PreviewTrack nt;
                    nt.box = cv::Rect2f(db.x + shift.x, db.y + shift.y,
                                        db.width, db.height);
                    clamp_box(nt.box);
                    nt.proto = d;
                    nt.last_match_ms = now;
                    nt.last_det_ts = dets_ts;
                    nt.pts = make_points(nt.box);
                    nt.hist.emplace_back(dets_ts, db);
                    nt.hist.emplace_back(now, nt.box);
                    preview_tracks_.push_back(std::move(nt));
                    used.push_back(1);
                }
            }
            // The attribute classifier can miss one pass on a far/small
            // person while the person is still walking inside the ROI.  Use
            // the stable full-frame person boxes as an anchor: keep the label
            // track if a person still overlaps it (classifier miss), and kill
            // it on this very snapshot if no person is there (the person left
            // the scene, so the box would otherwise become the edge ghost).
            {
                for (size_t i = 0; i < preview_tracks_.size(); ++i) {
                    if (used[i]) continue;
                    auto& t = preview_tracks_[i];
                    // An empty snapshot (no attribute hit and no person) is not
                    // evidence that the person left; only the time-based grace
                    // above may retire the track.
                    if (person_boxes.empty()) continue;
                    cv::Rect2f ref = t.box;
                    hist_at(t, dets_ts, ref);
                    int best_pb = -1;
                    float best_score = -1e9f;
                    for (size_t pi = 0; pi < person_boxes.size(); ++pi) {
                        auto& pb = person_boxes[pi];
                        float ix1 = std::max(pb.x, ref.x);
                        float iy1 = std::max(pb.y, ref.y);
                        float ix2 = std::min(pb.x + pb.width, ref.x + ref.width);
                        float iy2 = std::min(pb.y + pb.height, ref.y + ref.height);
                        float inter = std::max(0.0f, ix2 - ix1) * std::max(0.0f, iy2 - iy1);
                        float uni = pb.width * pb.height + ref.width * ref.height - inter;
                        float iou = uni > 0 ? inter / uni : 0.0f;
                        float dcx = (pb.x + pb.width * 0.5f) -
                                    (ref.x + ref.width * 0.5f);
                        float dcy = (pb.y + pb.height * 0.5f) -
                                    (ref.y + ref.height * 0.5f);
                        float diag = std::sqrt(ref.width * ref.width +
                                               ref.height * ref.height);
                        float cdist = diag > 1.0f
                                          ? std::sqrt(dcx * dcx + dcy * dcy) / diag
                                          : 0.0f;
                        if (iou < 0.15f && cdist > 0.55f) continue;
                        float score = iou - 0.1f * cdist;
                        if (score > best_score) {
                            best_score = score;
                            best_pb = static_cast<int>(pi);
                        }
                    }
                    if (best_pb < 0) {
                        // No person overlaps this label box in this snapshot.
                        // One empty/false-negative pass must not blank the
                        // box (that is what made labels blink at 1~2 Hz once
                        // detection ran at 5 Hz); give the anchor a short
                        // grace, then drop it so a person who really left does
                        // not leave an edge ghost behind.
                        int64_t base = t.last_person_ms > 0 ? t.last_person_ms
                                                            : t.last_match_ms;
                        if (dets_ts - base > person_grace_ms_) t.dead = true;
                        continue;
                    }
                    t.last_person_ms = dets_ts;
                    // The attribute classifier missed this snapshot, but a
                    // full-frame person is still there.  Snap the label onto
                    // that person's pose on the detection frame.  Applying a
                    // delta from the historical pose would preserve any drift
                    // that happened after that frame (the old box would stay
                    // on the empty aisle while the person walked right).
                    auto& pb = person_boxes[best_pb];
                    t.box = pb;
                    clamp_box(t.box);
                    t.pts = make_points(t.box);
                }
                size_t w = 0;
                for (size_t i = 0; i < preview_tracks_.size(); ++i) {
                    if (preview_tracks_[i].dead) continue;
                    if (w != i) preview_tracks_[w] = std::move(preview_tracks_[i]);
                    ++w;
                }
                preview_tracks_.resize(w);
            }
            preview_last_det_ts_ = dets_ts;
        }
        // Output one Detection per live track.  The ROI test is applied to the
        // *current* tracked centre on every preview frame, so a person who
        // crosses the green line loses the box on that frame (not 1s later).
        // Several algorithms can hit the same person (helmet + vest + life
        // jacket).  Production stacks those labels on ONE box; without merging
        // the tracks, each algorithm drew its own slightly different rectangle
        // and the preview showed nested/offset boxes ("标签乱飞").
        struct Cluster {
            cv::Rect2f box;
            std::vector<size_t> members;
        };
        std::vector<Cluster> clusters;
        for (size_t i = 0; i < preview_tracks_.size(); ++i) {
            auto& t = preview_tracks_[i];
            if (now - t.last_match_ms > stale_limit) continue;
            if (!center_in_roi(t.box)) continue;
            int hit = -1;
            for (size_t c = 0; c < clusters.size(); ++c) {
                auto& a = clusters[c].box;
                auto& b = t.box;
                float ix1 = std::max(a.x, b.x), iy1 = std::max(a.y, b.y);
                float ix2 = std::min(a.x + a.width, b.x + b.width);
                float iy2 = std::min(a.y + a.height, b.y + b.height);
                float inter = std::max(0.0f, ix2 - ix1) * std::max(0.0f, iy2 - iy1);
                float uni = a.width * a.height + b.width * b.height - inter;
                if (uni > 0 && inter / uni >= 0.45f) { hit = static_cast<int>(c); break; }
            }
            if (hit < 0) {
                clusters.push_back({t.box, {i}});
            } else {
                auto& c = clusters[hit];
                c.members.push_back(i);
                // Keep the largest rectangle: the person box covers every
                // algorithm's crop, so the stacked labels sit on one full box.
                if (t.box.width * t.box.height > c.box.width * c.box.height)
                    c.box = t.box;
            }
        }
        std::vector<Detection> out;
        for (auto& c : clusters) {
            bool any_confirmed = false;
            for (size_t idx : c.members)
                if (preview_tracks_[idx].proto.confirmed) any_confirmed = true;
            // One box, one state: if any algorithm confirmed the target, draw
            // the confirmed labels only.  Otherwise draw a single blue
            // countdown (the most advanced one), not one per algorithm — the
            // overlapping "3s"/"5s" digits came from emitting every member.
            size_t pending_pick = c.members.front();
            for (size_t idx : c.members) {
                auto& t = preview_tracks_[idx];
                if (t.proto.countdown_s < preview_tracks_[pending_pick].proto.countdown_s)
                    pending_pick = idx;
            }
            auto emit = [&](size_t idx) {
                auto& t = preview_tracks_[idx];
                Detection d = t.proto;
                d.x1 = c.box.x / sx;
                d.y1 = c.box.y / sy;
                d.x2 = std::min(static_cast<float>(frame.width()),
                                static_cast<float>((c.box.x + c.box.width) / sx));
                d.y2 = std::min(static_cast<float>(frame.height()),
                                static_cast<float>((c.box.y + c.box.height) / sy));
                out.push_back(std::move(d));
            };
            if (!any_confirmed) {
                emit(pending_pick);
                continue;
            }
            for (size_t idx : c.members)
                if (preview_tracks_[idx].proto.confirmed) emit(idx);
        }
        preview_prev_gray_ = gray;
        preview_prev_ts_ = now;
        return out;
    }
    void maybe_preview(sail::BMImage& frame, sail::Handle& handle,
                       sail::Bmcv& bmcv, int64_t now) {
        if (no_preview_) return;
        // Runtime switch for CPU profiling: /tmp/aidetect_no_preview disables
        // the preview/merge rendering path without restarting the service.
        if (std::ifstream("/tmp/aidetect_no_preview").good()) return;
        bool want_single = want_single_ ? want_single_() : true;
        bool want_tile = want_tile_ ? want_tile_() : false;
        bool forced_area = force_area_.exchange(false);
        if (forced_area) {
            try {
                Size2 sz = preview_target(frame.width(), frame.height());
                auto raw = render_preview_fast(frame, bmcv, sz.w, sz.h, {}, false, false);
                if (!raw.empty()) {
                    std::lock_guard<std::mutex> lk(area_jpeg_mutex_);
                    latest_area_jpeg_ =
                        std::make_shared<std::vector<unsigned char>>(raw);
                    last_area_ms_ = now;
                }
            } catch (const std::exception& e) {
                log_warn(cfg_.task_id + " area raw render: " + e.what());
            }
        }
        if (!want_single && !want_tile) return;
        bool expected = false;
        if (!preview_busy_.compare_exchange_strong(expected, true)) return;
        // Single-channel and merge tiles have separate budgets: a 3x3 merge
        // asks nine tasks for a tile on every frame, and those tile encodes
        // compete with inference for the single shared JPU/VPU.  Rendering
        // tiles at a slightly lower rate keeps the merge wall clock realtime
        // without touching the single-channel 10fps path.
        bool due_single = want_single && preview_due(now, false);
        bool due_tile = want_tile && preview_due(now, true);
        if (!due_single && !due_tile) {
            preview_busy_ = false;
            return;
        }
        want_single = due_single;
        want_tile = due_tile;
        std::vector<Detection> dets;
        std::vector<Detection> persons;
        int64_t selected_ts = 0;
        size_t pv_dets = 0, pv_persons = 0;
        int64_t pv_age = 0;
        {
            std::lock_guard<std::mutex> lk(det_mutex_);
            // Render boxes that belong to a frame no newer than the preview
            // frame.  The inference thread can finish a newer frame first;
            // applying its boxes to an older preview frame makes the boxes
            // jump/lead the person.  Prefer the newest snapshot at/before the
            // preview timestamp, then extrapolate to `now`.
            dets = dets_;
            persons = dets_persons_;
            selected_ts = dets_ms_;
            for (auto it = dets_history_.rbegin(); it != dets_history_.rend(); ++it) {
                if (it->ts <= now + 30) {
                    dets = it->dets;
                    persons = it->persons;
                    selected_ts = it->ts;
                    break;
                }
            }
            int64_t age = now - selected_ts;
            // If inference has completely stalled there is no frame-bound
            // evidence left for these boxes; hide them instead of drawing a
            // stale ghost.  Normal gaps are bridged by the frame tracker.
            if (age > 1500) {
                dets.clear();
                persons.clear();
            }
            pv_age = age;
            pv_dets = dets.size();
            pv_persons = persons.size();
        }
        if (profiling()) {
            const int64_t t = now_ms();
            if (t - last_prof_ms_ >= 2000) {
                last_prof_ms_ = t;
                log_info(cfg_.task_id + " preview dets=" + std::to_string(pv_dets) +
                         " persons=" + std::to_string(pv_persons) + " age=" +
                         std::to_string(pv_age) + " tracks=" +
                         std::to_string(preview_tracks_.size()));
            }
        }
        const int64_t pv_t0 = now_ms();
        // One scaled frame per preview tick: tracking, single preview and the
        // merge tile all work on it.  Production keeps a scaled frame per task
        // (DecodeInstance::requireScaled) for the same reason — resizing a 4K
        // frame three times per tick cost ~110ms for tracking + 30~270ms for
        // the render, which capped the preview at 2~3fps with 33 tasks.
        const Size2 pvsz = preview_target(frame.width(), frame.height());
        // bmcv.vpp_resize on this SDK only produces correct pixels for an
        // integer downscale factor.  2560x1440 -> 1920x1080 (x0.75) came back
        // with a corrupted bottom band, while 2560x1440 -> 1280x720 and
        // 3840x2160 -> 1920x1080 were clean.  Scale with VPP only when the
        // ratio is integral; otherwise keep the source frame and let the CPU
        // resize inside render_jpeg / render_preview_fast do the scaling.
        const bool need_scale =
            (frame.width() != pvsz.w || frame.height() != pvsz.h);
        const bool integral_scale =
            pvsz.w > 0 && pvsz.h > 0 && frame.width() % pvsz.w == 0 &&
            frame.height() % pvsz.h == 0;
        const bool vpp_scale = need_scale && integral_scale;
        const float pv_scale =
            vpp_scale ? static_cast<float>(pvsz.w) / std::max(1, frame.width())
                      : 1.0f;
        for (auto& d : dets) {
            d.x1 *= pv_scale;
            d.y1 *= pv_scale;
            d.x2 *= pv_scale;
            d.y2 *= pv_scale;
        }
        for (auto& d : persons) {
            d.x1 *= pv_scale;
            d.y1 *= pv_scale;
            d.x2 *= pv_scale;
            d.y2 *= pv_scale;
        }
        // Reuse one preview frame per task: production keeps pooled frames and
        // a fresh BMImage per tick churned the BM allocator.
        const int pw = vpp_scale ? pvsz.w : frame.width();
        const int ph = vpp_scale ? pvsz.h : frame.height();
        try {
            if (preview_img_w_ != pw || preview_img_h_ != ph) {
                preview_img_ = sail::BMImage(handle, ph, pw, frame.format(),
                                             DATA_TYPE_EXT_1N_BYTE);
                preview_img_w_ = pw;
                preview_img_h_ = ph;
            }
            if (vpp_scale) {
                if (bmcv.vpp_resize(frame, preview_img_, pvsz.w, pvsz.h,
                                    bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0) {
                    preview_busy_ = false;
                    return;
                }
            } else if (bmcv.image_copy_to(frame, preview_img_, 0, 0) != 0) {
                preview_busy_ = false;
                return;
            }
        } catch (const std::exception& e) {
            log_warn(cfg_.task_id + " preview scale: " + std::string(e.what()));
            preview_busy_ = false;
            return;
        }
        if (!dets.empty() || !persons.empty() || !preview_tracks_.empty()) {
            // Match the halving rule inside track_preview_boxes so the pooled
            // scratch image is exactly the size it will resize into.
            const int fw0 = preview_img_.width();
            const int fh0 = preview_img_.height();
            int tw0 = fw0;
            while (tw0 > 1280 && tw0 % 2 == 0) tw0 /= 2;
            int th0 = static_cast<int>(std::lround(
                fh0 * static_cast<double>(tw0) / std::max(1, fw0)));
            tw0 &= ~1;
            th0 &= ~1;
            if (tw0 < 2) tw0 = 2;
            if (th0 < 2) th0 = 2;
            if (track_img_w_ != tw0 || track_img_h_ != th0) {
                track_img_ = sail::BMImage(handle, th0, tw0, preview_img_.format(),
                                           DATA_TYPE_EXT_1N_BYTE);
                track_img_w_ = tw0;
                track_img_h_ = th0;
            }
            dets = track_preview_boxes(preview_img_, track_img_, bmcv,
                                       std::move(dets), std::move(persons),
                                       selected_ts, now);
        }
        const int64_t pv_t_track = now_ms();
        try {
            {
                std::lock_guard<std::mutex> lk(preview_mu_);
                preview_frame_ptr_ = &preview_img_;
                preview_dets_ = std::move(dets);
                preview_want_single_ = want_single;
                preview_want_tile_ = want_tile;
                preview_has_work_ = true;
            }
            preview_cv_.notify_one();
            if (profiling() && pv_t_track - last_pvprof_ms_ >= 2000) {
                last_pvprof_ms_ = pv_t_track;
                log_info(cfg_.task_id + " preview path track_ms=" +
                         std::to_string(pv_t_track - pv_t0) + " out=" +
                         std::to_string(pvsz.w) + "x" + std::to_string(pvsz.h) +
                         " src=" + std::to_string(frame.width()) + "x" +
                         std::to_string(frame.height()));
            }
        } catch (const std::exception& e) {
            preview_busy_ = false;
            log_error(cfg_.task_id + " preview queue: " + e.what());
        }
    }
    void preview_loop() {
        sail::Handle handle(0);
        sail::Bmcv bmcv(handle);
        while (!stop_) {
            sail::BMImage* frame = nullptr;
            std::vector<Detection> dets;
            bool want_single = false, want_tile = false;
            {
                std::unique_lock<std::mutex> lk(preview_mu_);
                preview_cv_.wait_for(lk, std::chrono::milliseconds(500),
                                     [this] { return stop_ || preview_has_work_; });
                if (stop_) break;
                if (!preview_has_work_ || !preview_frame_ptr_) continue;
                frame = preview_frame_ptr_;
                dets = preview_dets_;
                preview_dets_.clear();
                want_single = preview_want_single_;
                want_tile = preview_want_tile_;
                preview_has_work_ = false;
            }
            try {
            if (!frame) continue;
                if (want_tile) {
                    auto jpeg = render_tile_fast(*frame, bmcv, handle, dets);
                    if (!jpeg.empty() && tile_cb_)
                        tile_cb_(cfg_.task_id, group_, slot_, jpeg);
                }
                if (want_single) {
                    const int64_t pv_render_t0 = now_ms();
                    Size2 sz = preview_target(frame->width(), frame->height());
                    bool any_pending = false;
                    for (auto& d : dets) if (d.pending) any_pending = true;
                    auto jpeg = any_pending
                        ? render_jpeg(*frame, bmcv, sz.w, sz.h, dets)
                        : render_preview_fast(*frame, bmcv, sz.w, sz.h, dets);
                    if (!jpeg.empty()) {
                        {
                            std::lock_guard<std::mutex> lk(jpeg_mutex_);
                            latest_jpeg_ = std::make_shared<std::vector<unsigned char>>(jpeg);
                        }
                        last_area_ms_ = now_ms();
                        if (single_cb_) single_cb_(cfg_.task_id, jpeg);
                        if (profiling() && now_ms() - pv_render_t0 > 0) {
                            log_info(cfg_.task_id + " preview render_ms=" +
                                     std::to_string(now_ms() - pv_render_t0) +
                                     " out=" + std::to_string(sz.w) + "x" +
                                     std::to_string(sz.h));
                        }
                    }
                }
            } catch (const std::exception& e) {
                log_error(cfg_.task_id + " preview worker: " + e.what());
            }
            preview_busy_ = false;
        }
        preview_busy_ = false;
    }
    // Low-resolution merge tile: VPP resize + bmcv rectangles + JPU JPEG, all on
    // the device. No CPU copy/label pass is needed for the 3x3 grid.
    std::vector<unsigned char> render_tile_fast(sail::BMImage& frame, sail::Bmcv& bmcv,
                                                sail::Handle& handle,
                                                const std::vector<Detection>& dets) {
        int tw = tile_w_, th = tile_h_;
        if (frame.width() <= 0 || frame.height() <= 0) return {};
        float sx = static_cast<float>(tw) / frame.width();
        float sy = static_cast<float>(th) / frame.height();
        try {
            if (tile_img_w_ != tw || tile_img_h_ != th) {
                tile_img_ = sail::BMImage(handle, th, tw, frame.format(),
                                          DATA_TYPE_EXT_1N_BYTE);
                tile_img_w_ = tw;
                tile_img_h_ = th;
            }
            // vpp_resize is only pixel-correct for an integer divisor, so crop
            // the source to an exact k*tile box (centred) and then downscale by
            // exactly k.  4096x2160 -> crop 3840x2160 -> /6 -> 640x360 is clean,
            // while 4096 -> 640 (x6.4) produced colour noise.
            int tk = std::min(frame.width() / std::max(1, tw),
                              frame.height() / std::max(1, th));
            if (tk < 1) tk = 1;
            const int cw = std::min(frame.width(), tw * tk);
            const int ch = std::min(frame.height(), th * tk);
            const int cx = (frame.width() - cw) / 2;
            const int cy = (frame.height() - ch) / 2;
            // Non-integer ratios (e.g. 4096x2160 -> 640x360, x6.4) are broken
            // on this SDK for every device-side scaler we tried, so use the CPU
            // renderer for them (few 4K sources, ~5-10 ms per tile).
            if (frame.width() % std::max(1, tw) != 0 ||
                frame.height() % std::max(1, th) != 0)
                return render_jpeg(frame, bmcv, tw, th, dets);
            bool tile_ok = false;
            try {
                tile_ok = bmcv.crop_and_resize(frame, tile_img_, cx, cy, cw, ch,
                                               tw, th,
                                               bmcv_resize_algorithm::BMCV_INTER_LINEAR) == 0;
            } catch (...) {
                tile_ok = false;
            }
            if (!tile_ok &&
                bmcv.vpp_resize(frame, tile_img_, tw, th,
                                bmcv_resize_algorithm::BMCV_INTER_LINEAR) != 0)
                return {};
            sail::BMImage bgr = bmcv.vpp_convert_format(tile_img_, FORMAT_BGR_PACKED);
            draw_roi_bm(bmcv, bgr, tw, th);
            for (auto& d : dets) {
                if (!d.target && !d.pending && !d.confirmed) continue;
                int x = static_cast<int>(d.x1 * sx);
                int y = static_cast<int>(d.y1 * sy);
                int w = static_cast<int>((d.x2 - d.x1) * sx);
                int h = static_cast<int>((d.y2 - d.y1) * sy);
                if (w < 1) w = 1;
                if (h < 1) h = 1;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (x >= tw) x = tw - 1;
                if (y >= th) y = th - 1;
                if (x + w > tw) w = tw - x;
                if (y + h > th) h = th - y;
                std::tuple<int, int, int> color =
                    d.pending ? std::make_tuple(255, 0, 0) : (d.target ? std::make_tuple(0, 0, 255) : std::make_tuple(0, 255, 0));
                bmcv.rectangle(bgr, x, y, w, h, color, 2);
            }
            {
                std::lock_guard<std::mutex> font_lock(font_mutex());
                auto bar = task_bar_bm(tw, th);
                if (bar && bar->width() > 0 && bar->height() > 0)
                    bmcv.image_copy_to(*bar, bgr, tw - bar->width(), 0);
            }
            std::vector<unsigned char> buf;
            {
                std::lock_guard<std::mutex> lk(jpeg_hw_mutex());
                std::string ext = ".jpg";
                if (!bmcv.imencode(ext, bgr, buf)) buf.clear();
            }
            // A truncated JPEG (encoder interrupted / frame recycled mid-encode)
            // would decode into colour noise.  Drop it here: single preview
            // keeps its previous frame, the merge keeps the previous tile.
            if (!buf.empty() && jpeg_markers_ok(buf)) return buf;
            if (!buf.empty()) {
                log_warn(cfg_.task_id + " jpeg truncated, keep previous frame");
                buf.clear();
            }
            return buf;
        } catch (const std::exception& e) {
            log_warn(cfg_.task_id + " tile preview: " + std::string(e.what()));
        }
        return render_jpeg(frame, bmcv, tw, th, dets);
    }
    // Single-channel preview: VPP downscale (never upscale) + JPU JPEG. Labels
    // reuse the same pre-rendered Chinese plates as the alarm images, copied on
    // the device, so no CPU Mat round-trip is needed at 10fps.
    std::shared_ptr<sail::BMImage> plate_bm(const std::string& label, int target_h) {
        auto src = label_plates_.find(label);
        if (src == label_plates_.end() || src->second.empty()) return nullptr;
        std::string key = label + "#" + std::to_string(target_h);
        auto it = plate_cache_.find(key);
        if (it != plate_cache_.end()) return it->second;
        double s = std::min(1.0, static_cast<double>(target_h) / 1080.0);
        cv::Mat m;
        if (s < 0.999) {
            int w = std::max(8, static_cast<int>(src->second.cols * s));
            int h = std::max(8, static_cast<int>(src->second.rows * s));
            cv::resize(src->second, m, cv::Size(w, h));
        } else {
            m = src->second;
        }
        auto bm = std::make_shared<sail::BMImage>(sail::Bmcv::mat_to_bm_image(m));
        plate_cache_[key] = bm;
        return bm;
    }
    std::vector<unsigned char> render_preview_fast(sail::BMImage& frame, sail::Bmcv& bmcv,
                                                   int tw, int th,
                                                   const std::vector<Detection>& dets,
                                                   bool with_task_bar = true,
                                                   bool with_roi = true) {
        if (frame.width() <= 0 || frame.height() <= 0) return {};
        float sx = static_cast<float>(tw) / frame.width();
        float sy = static_cast<float>(th) / frame.height();
        try {
            sail::BMImage scaled = bmcv.vpp_resize(
                frame, tw, th, bmcv_resize_algorithm::BMCV_INTER_LINEAR);
            sail::BMImage bgr = bmcv.vpp_convert_format(scaled, FORMAT_BGR_PACKED);
            if (with_roi) draw_roi_bm(bmcv, bgr, tw, th);
            std::vector<std::tuple<int, int, int, int>> label_rects;
            for (auto& d : dets) {
                if (!d.target && !d.pending && !d.confirmed) continue;
                int x = static_cast<int>(d.x1 * sx);
                int y = static_cast<int>(d.y1 * sy);
                int w = static_cast<int>((d.x2 - d.x1) * sx);
                int h = static_cast<int>((d.y2 - d.y1) * sy);
                if (w < 1) w = 1;
                if (h < 1) h = 1;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (x >= tw) x = tw - 1;
                if (y >= th) y = th - 1;
                if (x + w > tw) w = tw - x;
                if (y + h > th) h = th - y;
                std::tuple<int, int, int> color =
                    d.pending ? std::make_tuple(255, 0, 0) : (d.target ? std::make_tuple(0, 0, 255) : std::make_tuple(0, 255, 0));
                int rrc = bmcv.rectangle(bgr, x, y, w, h, color, 2);
                if (rrc != 0 && !rect_warned_) {
                    log_warn(cfg_.task_id + " bmcv.rectangle rc=" + std::to_string(rrc));
                    rect_warned_ = true;
                }
                if (d.pending) continue;
                auto plate = plate_bm(d.label, th);
                if (plate) {
                    int px = std::max(0, std::min(x, tw - plate->width()));
                    // First label bottom touches the box top; extra labels
                    // stack vertically upward, matching production.
                    int py = y - plate->height();
                    for (int guard = 0; guard < 12; ++guard) {
                        bool hit = false;
                        for (auto& r : label_rects) {
                            int rx = std::get<0>(r), ry = std::get<1>(r);
                            int rw = std::get<2>(r), rh = std::get<3>(r);
                            if (px < rx + rw && px + plate->width() > rx &&
                                py < ry + rh && py + plate->height() > ry) {
                                hit = true;
                                break;
                            }
                        }
                        if (!hit) break;
                        py -= plate->height();
                    }
                    py = std::max(0, std::min(py, th - plate->height()));
                    label_rects.emplace_back(px, py, plate->width(), plate->height());
                    int crc = bmcv.image_copy_to(*plate, bgr, px, py);
                    if (crc != 0 && !plate_warned_) {
                        log_warn(cfg_.task_id + " plate copy rc=" + std::to_string(crc));
                        plate_warned_ = true;
                    }
                }
            }
            if (with_task_bar) {
                std::lock_guard<std::mutex> font_lock(font_mutex());
                auto bar = task_bar_bm(tw, th);
                if (bar && bar->width() > 0 && bar->height() > 0)
                    bmcv.image_copy_to(*bar, bgr, tw - bar->width(), 0);
            }
            std::vector<unsigned char> buf;
            {
                std::lock_guard<std::mutex> lk(jpeg_hw_mutex());
                std::string ext = ".jpg";
                if (!bmcv.imencode(ext, bgr, buf)) buf.clear();
            }
            // A truncated JPEG (encoder interrupted / frame recycled mid-encode)
            // would decode into colour noise.  Drop it here: single preview
            // keeps its previous frame, the merge keeps the previous tile.
            if (!buf.empty() && jpeg_markers_ok(buf)) return buf;
            if (!buf.empty()) {
                log_warn(cfg_.task_id + " jpeg truncated, keep previous frame");
                buf.clear();
            }
            return buf;
        } catch (const std::exception& e) {
            log_warn(cfg_.task_id + " fast preview: " + std::string(e.what()));
        }
        return render_jpeg(frame, bmcv, tw, th, dets);
    }
    // Image ENV has AIDETECT_NO_DUMP=1 baked in; treat "0" as "dump allowed"
    // so the container can enable sail dump / bridge clips without a rebuild.
    static bool no_dump_env() {
        const char* v = std::getenv("AIDETECT_NO_DUMP");
        return v && *v && std::string(v) != "0";
    }
    int dump_in_worker(const std::string& url, int pre, int post,
                       const std::string& out) {
        char exe[PATH_MAX] = {0};
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (n <= 0) return -1;
        auto quote = [](const std::string& s) {
            std::string q = "'";
            for (char c : s) {
                if (c == '\'') q += "'\\''";
                else q += c;
            }
            q += "'";
            return q;
        };
        std::string cmd = quote(exe) + " --dump-only --url " + quote(url) +
                          " --out " + quote(out) + " --pre " +
                          std::to_string(std::max(1, pre)) + " --post " +
                          std::to_string(std::max(1, post)) + " >/tmp/aidetect-dump.log 2>&1";
        int status = std::system(cmd.c_str());
        if (status == -1) return -1;
        if (!WIFEXITED(status)) return -1;
        return WEXITSTATUS(status);
    }
    std::string probe_codec(const std::string& url) {
        std::string cmd = "ffprobe -v error -select_streams v:0 "
                          "-show_entries stream=codec_name "
                          "-of default=nw=1:nk=1 '" + url + "' 2>/dev/null";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) return "";
        char line[64] = {0};
        std::string codec;
        if (fgets(line, sizeof(line), p)) {
            codec = line;
            while (!codec.empty() &&
                   (codec.back() == '\n' || codec.back() == '\r'))
                codec.pop_back();
        }
        pclose(p);
        return codec;
    }
    double probe_fps(const std::string& url) {
        std::string cmd = "ffprobe -v error -select_streams v:0 "
                          "-show_entries stream=avg_frame_rate "
                          "-of default=nw=1:nk=1 '" + url + "' 2>/dev/null";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) return 0;
        char line[64] = {0};
        double fps = 0;
        if (fgets(line, sizeof(line), p)) {
            int n = 0, d = 0;
            if (sscanf(line, "%d/%d", &n, &d) == 2 && d > 0) fps = double(n) / d;
            else {
                try { fps = std::stod(line); } catch (...) {}
            }
        }
        pclose(p);
        return fps;
    }
    // Source frame rate drives the preview policy: <=10fps passes every frame
    // through untouched, >10fps is decimated to 10fps by the token bucket.
    void probe_source_fps() {
        if (source_url_.empty()) return;
        src_fps_ = probe_fps(source_url_);
        log_info(cfg_.task_id + " source fps=" + std::to_string(src_fps_));
    }
    // One timestamped encoded-packet ring per task (production: libdump4's
    // FrameBuffering + Mp4V2Writer).  The decoder and this recorder each pull
    // the stream, so ISC gets a fresh previewURL for the recorder (separate
    // session) instead of sharing the decoder's one-shot openUrl.
    // One upstream connection per channel (production's media-source model):
    // ffmpeg pulls the camera/ISC RTSP once and fans out two local MPEG-TS
    // streams. The decoder reads one, the alarm packet ring reads the other.
    // This removes ISC one-session conflicts and the 2s skew between a separate
    // decoder connection and recorder connection.
    static bool udp_has_data(int port, int timeout_ms) {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) return false;
        timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<uint16_t>(port));
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bool ok = false;
        if (::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0) {
            char buf[2048];
            ok = ::recv(fd, buf, sizeof(buf), 0) > 0;
        }
        ::close(fd);
        return ok;
    }
    static bool udp_port_free(int port) {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) return false;
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<uint16_t>(port));
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bool ok = ::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0;
        ::close(fd);
        return ok;
    }
    static std::mutex& gateway_ports_mutex() {
        static std::mutex m;
        return m;
    }
    static std::set<int>& gateway_ports() {
        static std::set<int> s;
        return s;
    }
    // Allocate a unique local UDP port pair. Deriving the port from group/slot
    // collided with orphan gateways left over from a previous process, which
    // then made the decoder fail with "Address already in use".
    void wake_decoder_udp() {
        if (port_dec_ <= 0) return;
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) return;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port_dec_));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        unsigned char pkt[188];
        std::memset(pkt, 0xff, sizeof(pkt));
        pkt[0] = 0x47;
        for (int i = 0; i < 6; ++i)
            (void)::sendto(fd, pkt, sizeof(pkt), 0,
                           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::close(fd);
    }
    bool alloc_gateway_ports(int& p_dec, int& p_rec) {
        std::lock_guard<std::mutex> lk(gateway_ports_mutex());
        auto& used = gateway_ports();
        for (int p = 22000; p < 23000; p += 2) {
            if (used.count(p) || used.count(p + 1)) continue;
            if (!udp_port_free(p) || !udp_port_free(p + 1)) continue;
            used.insert(p);
            used.insert(p + 1);
            p_dec = p;
            p_rec = p + 1;
            return true;
        }
        return false;
    }
    void release_gateway_ports() {
        if (port_dec_ <= 0 && port_rec_ <= 0) return;
        std::lock_guard<std::mutex> lk(gateway_ports_mutex());
        auto& used = gateway_ports();
        used.erase(port_dec_);
        used.erase(port_rec_);
        port_dec_ = 0;
        port_rec_ = 0;
    }
    static std::string tail_file(const std::string& path, size_t max_bytes) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return {};
        f.seekg(0, std::ios::end);
        auto len = static_cast<size_t>(f.tellg());
        size_t start = len > max_bytes ? len - max_bytes : 0;
        f.seekg(static_cast<std::streamoff>(start));
        std::string out((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
        return out;
    }
        // Production decodes the channel's configured stream and its pipeline
        // handles H.265.  Our gateway muxes with -c copy into MPEG-TS, which
        // needs Annex-B NALs for HEVC (sophon ffmpeg 0.15 does not insert
        // hevc_mp4toannexb itself).  The codec is learned from the gateway log
        // on the first attempt, then the gateway retries with the filter.
    bool start_gateway(const std::string& upstream) {
        last_progress_ms_ = now_ms();
        stop_gateway();
        int p_dec = 0, p_rec = 0;
        if (!alloc_gateway_ports(p_dec, p_rec)) {
            log_warn(cfg_.task_id + " no free local UDP port for gateway");
            return false;
        }
        port_dec_ = p_dec;
        port_rec_ = p_rec;
        upstream_url_ = upstream;
        // 4K H.264 bursts can overflow a 1 MB UDP socket; the decoder then
        // paints a torn/colorful bottom strip.  Keep a larger receive buffer
        // and tolerate overruns instead of losing packets.
        std::string dec = "udp://127.0.0.1:" + std::to_string(p_dec) +
                          "?pkt_size=1316&buffer_size=4194304&overrun_nonfatal=1";
        std::string rec = "udp://127.0.0.1:" + std::to_string(p_rec) +
                          "?pkt_size=1316&buffer_size=4194304&overrun_nonfatal=1";
        std::string dir = app_.tmp_root + "/" + cfg_.task_id;
        ensure_dir(dir);
        std::string pid_file = dir + "/gateway.pid";
        std::string log_file = "/tmp/" + cfg_.task_id + "-gateway.log";
        bool rtsp = upstream.rfind("rtsp://", 0) == 0;
        bool paced = upstream.rfind("rtsp://", 0) == 0 ||
                     upstream.rfind("rtmp://", 0) == 0 ||
                     upstream.rfind("udp://", 0) == 0;
        std::vector<std::string> args{"ffmpeg", "-y", "-nostdin", "-hide_banner", "-loglevel",
                                      upstream_hevc_ < 0 ? "info" : "error"};
        if (rtsp) {
            args.push_back("-rtsp_transport");
            args.push_back("tcp");
        }
        if (!paced) args.push_back("-re");
        if (file_source_) {
            // MP4 test mode: loop forever at the source, so decoder/process/
            // preview/detect all stay identical to a live stream.
            args.push_back("-stream_loop");
            args.push_back("-1");
        }
        args.push_back("-i");
        args.push_back(upstream);
        if (gateway_bsf_hevc_ && upstream_hevc_ == 1) {
            // Output option: HEVC must be Annex-B inside MPEG-TS.
            args.push_back("-bsf:v");
            args.push_back("hevc_mp4toannexb");
        }
        args.insert(args.end(), {"-map", "0:v", "-c", "copy", "-f", "mpegts", dec,
                                 "-map", "0:v", "-c", "copy", "-f", "mpegts", rec});
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        // Fork/exec instead of posix_spawn: the child must close every
        // inherited AI socket. Otherwise a gateway keeps a previous task's
        // UDP decoder port bound (EADDRINUSE) and only a few channels work.
        if (inproc_ingest_enabled()) {
            // Same contract as the ffmpeg child: pull the source, mux MPEG-TS,
            // fan out to the decoder port and the packet-ring port.
            bridge_.start(upstream, p_dec, p_rec,
                          gateway_bsf_hevc_ && upstream_hevc_ == 1, file_source_);
            source_url_ = dec;
            ring_url_ = rec;
            started_ms_ = now_ms();
            gateway_pid_ = "inproc";
            bool got = false;
            const int attempts = 10;
            for (int i = 0; i < attempts && !got; ++i) {
                got = udp_has_data(p_dec, 4000);
                if (!got && !bridge_.running()) break;
                if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            if (!got) {
                const std::string err = bridge_.error();
                log_warn(cfg_.task_id + " ingest bridge no video: " +
                         (err.empty() ? upstream : err));
                set_stream_error(err.empty() ? ("upstream no video: " + upstream) : err);
                stop_gateway();
                return false;
            }
            log_info(cfg_.task_id + " ingest bridge pid=none udp " +
                     std::to_string(p_dec) + "/" + std::to_string(p_rec));
            const std::string seg_dir = app_.tmp_root + "/" + cfg_.task_id + "/segs";
            ensure_dir(seg_dir);
            seg_rec_.start(p_rec, seg_dir, 4, 6);
            return true;
        }
        pid_t pid = fork();
        if (pid == 0) {
            setsid();
            int devnull = ::open("/dev/null", O_RDONLY);
            if (devnull >= 0) { ::dup2(devnull, 0); if (devnull > 2) ::close(devnull); }
            int logfd = ::open(log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (logfd >= 0) {
                ::dup2(logfd, 1);
                ::dup2(logfd, 2);
                if (logfd > 2) ::close(logfd);
            }
            for (int fd = 3; fd < 1024; ++fd) ::close(fd);
            ::execvp("ffmpeg", argv.data());
            _exit(127);
        }
        if (pid < 0) {
            log_warn(cfg_.task_id + " gateway fork failed: " + std::string(std::strerror(errno)));
            set_stream_error("gateway fork failed: " + std::string(std::strerror(errno)));
            release_gateway_ports();
            return false;
        }
        gateway_pid_num_ = pid;
        gateway_pid_ = std::to_string(pid);
        started_ms_ = now_ms();
        {
            std::ofstream pf(pid_file);
            pf << gateway_pid_;
        }
        source_url_ = dec;
        ring_url_ = rec;
        // UDP has no connect timeout: wait for the first local datagram instead
        // of letting sail::Decoder block forever. RTMP transcode setup can be
        // slower than RTSP, so give it more time before falling back.
        bool got = false;
        int attempts = upstream_hevc_ < 0 ? 2 : (rtsp ? 7 : 10);
        for (int i = 0; i < attempts && !got; ++i) {
            got = udp_has_data(p_dec, 4000);
            int status = 0;
            pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == pid) {
                std::string ge = tail_file(log_file, 600);
                log_warn(cfg_.task_id + " gateway exited before video: " + ge);
                set_stream_error(ge.empty() ? "gateway exited before video" : ge);
                break;
            }
            if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if (!got) {
            if (upstream_hevc_ < 0) {
                const std::string ge = tail_file(log_file, 4000);
                // Only classify when the probe actually printed a codec line;
                // an RTMP/DESCRIBE failure says nothing about the codec.
                if (ge.find("Video: hevc") != std::string::npos ||
                    ge.find("Video: h265") != std::string::npos)
                    upstream_hevc_ = 1;
                else if (ge.find("Video: h264") != std::string::npos)
                    upstream_hevc_ = 0;
            }
            log_warn(cfg_.task_id + " gateway no video stream: " + upstream);
            set_stream_error("upstream no video: " + upstream);
            stop_gateway();
            return false;
        }
        // Frames arrived without the HEVC filter, so this source does not need it.
        if (upstream_hevc_ < 0) upstream_hevc_ = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        log_info(cfg_.task_id + " gateway pid=" + gateway_pid_ + " udp " +
                 std::to_string(p_dec) + "/" + std::to_string(p_rec));
        set_stream_error("");
        return true;
    }
    // ISC preview URLs are single-use and camera streams differ: some cameras
    // only work on the main stream, others on the sub stream; RTMP is the
    // vendor's protocol but a few boxes/ports only allow RTSP. Try a short,
    // ordered candidate list instead of retrying the same URL forever.
    // Single-process ingest: sail::Decoder can open an rtsp:// URL or a local
    // file directly (verified on this box), so the external ffmpeg gateway is
    // optional.  Production keeps one decode instance per channel in-process;
    // this is the same model and removes 30+ helper processes + the UDP hop.
    bool direct_decode_enabled() const {
        static const bool on = [] {
            const char* v = std::getenv("AIDETECT_DIRECT_DECODE");
            if (v && std::string(v) == "0") return false;
            // /tmp/aidetect_no_direct switches every channel to the ingest
            // bridge (needed for alarm clips: the bridge keeps the ES ring).
            // Toggle without recreating the container.
            if (std::ifstream("/tmp/aidetect_no_direct").good()) return false;
            return true;
        }();
        return on;
    }
    std::string direct_decode_url() {
        if (cfg_.channel_address.rfind("isc://", 0) != 0)
            return cfg_.channel_address;
        // ISC: walk the same candidate list and hand the first resolvable URL
        // to the decoder instead of spawning a gateway.
        int base_st = IscResolver::stream_type_of(cfg_.channel_address);
        std::string base_proto =
            cfg_.channel_address.find("protocol=rtmp") != std::string::npos ? "rtmp" : "rtsp";
        std::vector<std::pair<std::string, int>> cands;
        auto add = [&](const std::string& proto, int st) {
            std::string p = (proto == "rtmp") ? "rtmp" : "rtsp";
            int s = st == 1 ? 1 : 0;
            for (auto& c : cands)
                if (c.first == p && c.second == s) return;
            cands.push_back({p, s});
        };
        add(base_proto, base_st);
        add(base_proto == "rtsp" ? "rtmp" : "rtsp", base_st);
        add(base_proto, base_st == 0 ? 1 : 0);
        add(base_proto == "rtsp" ? "rtmp" : "rtsp", base_st == 0 ? 1 : 0);
        if (isc_rot_ > 0 && !cands.empty()) {
            const size_t k = static_cast<size_t>(isc_rot_) % cands.size();
            std::rotate(cands.begin(), cands.begin() + k, cands.end());
        }
        for (auto& c : cands) {
            last_progress_ms_ = now_ms();
            try {
                std::string url =
                    IscResolver::resolve(cfg_.channel_address, c.first, c.second);
                if (!url.empty()) {
                    log_info(cfg_.task_id + " direct decode " + c.first +
                             " streamType=" + std::to_string(c.second));
                    return url;
                }
            } catch (const std::exception& e) {
                log_warn(cfg_.task_id + " ISC direct " + c.first + "/" +
                         std::to_string(c.second) + " failed: " + e.what());
            }
        }
        ++isc_rot_;
        return {};
    }

    bool start_isc_gateway() {
        int base_st = IscResolver::stream_type_of(cfg_.channel_address);
        // Someone watching the single channel -> main stream; otherwise the
        // substream, which is ~10x cheaper to decode on this box.
        const int pref = isc_stream_pref_.load();
        if (pref >= 0) base_st = pref;
        // Production MediaUrl uses protocol=rtmp&transcode=1. Prefer the
        // channel's protocol; the old environment override forced rtsp first
        // and hit the ISC 500/session limit on many cameras.
        std::string base_proto =
            cfg_.channel_address.find("protocol=rtmp") != std::string::npos ? "rtmp" : "rtsp";
        if (const char* p = std::getenv("AIDETECT_ISC_PROTOCOL"); p && *p)
            if (cfg_.channel_address.find("protocol=") == std::string::npos)
                base_proto = p;
        if (base_proto != "rtsp" && base_proto != "rtmp") base_proto = "rtsp";
        std::vector<std::pair<std::string, int>> cands;
        auto add = [&](const std::string& proto, int st) {
            std::string p = (proto == "rtmp") ? "rtmp" : "rtsp";
            int s = st == 1 ? 1 : 0;
            for (auto& c : cands)
                if (c.first == p && c.second == s) return;
            cands.push_back({p, s});
        };
        // Production keeps one decoded stream per channel and uses the
        // streamType stored in the media record (verified in the production
        // DB: some ISC cameras are streamType=0, e.g. 25K is 1 = main).
        // Order: configured streamType first, then the other one as a
        // fallback, over the configured protocol first.
        cands.clear();
        add(base_proto, base_st);
        add(base_proto == "rtsp" ? "rtmp" : "rtsp", base_st);
        add(base_proto, base_st == 0 ? 1 : 0);
        add(base_proto == "rtsp" ? "rtmp" : "rtsp", base_st == 0 ? 1 : 0);
        if (isc_rot_ > 0 && !cands.empty()) {
            // A streamType whose codec this decoder cannot open would otherwise
            // be retried forever: rotate so the next attempt starts elsewhere.
            const size_t k = static_cast<size_t>(isc_rot_) % cands.size();
            std::rotate(cands.begin(), cands.begin() + k, cands.end());
        }
        for (auto& c : cands) {
            last_progress_ms_ = now_ms();
            // A HEVC attempt must not force the bitstream filter onto the
            // next (possibly H.264) candidate: that makes ffmpeg exit at once,
            // which is what kept several ISC channels dark.
            gateway_bsf_hevc_ = false;
            try {
                std::string url = IscResolver::resolve(cfg_.channel_address, c.first, c.second);
                log_info(cfg_.task_id + " ISC candidate " + c.first +
                         " streamType=" + std::to_string(c.second));
                if (start_gateway(url)) {
                    upstream_url_ = url;
                    return true;
                }
                if (upstream_hevc_ == 1 && !gateway_bsf_hevc_) {
                    gateway_bsf_hevc_ = true;
                    log_info(cfg_.task_id +
                             " HEVC source, retry with hevc_mp4toannexb");
                    std::string url2 =
                        IscResolver::resolve(cfg_.channel_address, c.first, c.second);
                    if (start_gateway(url2)) {
                        upstream_url_ = url2;
                        return true;
                    }
                gateway_bsf_hevc_ = false;
                }
            } catch (const std::exception& e) {
                log_warn(cfg_.task_id + " ISC " + c.first + "/" +
                         std::to_string(c.second) + " failed: " + e.what());
                set_stream_error(std::string("ISC: ") + e.what());
            }
        }
        return false;
    }
    void stop_gateway() {
        seg_rec_.stop();
        bridge_.stop();
        if (gateway_pid_num_ > 0) {
            ::kill(-gateway_pid_num_, SIGKILL);
            ::kill(gateway_pid_num_, SIGKILL);
            int status = 0;
            for (int i = 0; i < 40; ++i) {
                pid_t w = waitpid(gateway_pid_num_, &status, WNOHANG);
                if (w == gateway_pid_num_ || (w < 0 && errno == ECHILD)) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
        gateway_pid_num_ = 0;
        gateway_pid_.clear();
        release_gateway_ports();
        source_url_.clear();
        ring_url_.clear();
    }
    void start_recorder() {
        // MP4 playback must never upload alarm events or write alarm clips.
        if (file_source_) return;
        if (std::getenv("AIDETECT_NO_RECORDER")) return;
        if (source_url_.empty()) return;
        // Single-decoder mode uses sail::Decoder::dump() on the same network
        // connection (production also keeps one stream per channel). The extra
        // packet-ring process is only used for multi mode, where dump crashes.
        bool want_ring = use_multi_;
        if (const char* v = std::getenv("AIDETECT_ALARM_RING"))
            want_ring = std::atoi(v) != 0;
        if (!want_ring) return;
        ring_dir_ = app_.data_root + "/" + cfg_.task_id + "/ring";
        ensure_dir(ring_dir_);
        std::string rec_url = ring_url_.empty() ? source_url_ : ring_url_;
        if (ring_url_.empty() && cfg_.channel_address.rfind("isc://", 0) == 0) {
            try {
                rec_url = IscResolver::resolve(cfg_.channel_address);
                log_info(cfg_.task_id + " recorder ISC url refreshed");
            } catch (const std::exception& e) {
                log_warn(cfg_.task_id + " recorder ISC resolve failed: " + e.what());
            }
        }
        int post = std::max(1, cfg_.video_duration - 5);
        int ring_mb = 32;
        if (const char* v = std::getenv("AIDETECT_REC_RING_MB"))
            ring_mb = std::max(8, std::atoi(v));
        recorder_ = std::make_unique<AlarmRingRecorder>();
        recorder_->start(rec_url, 5, post, 5, ring_mb);
        log_info(cfg_.task_id + " alarm packet ring started pre=5 post=" +
                 std::to_string(post) + " ring_mb=" + std::to_string(ring_mb));
    }
    void stop_recorder() {
        if (!recorder_) return;
        recorder_->stop();
        recorder_.reset();
    }
    void restart_recorder() {
        stop_recorder();
        start_recorder();
    }
    int clip_recorder(int64_t alarm_ms, int post, const std::string& out) {
        if (!recorder_ || !recorder_->running()) return -1;
        // Both the decoder and the ring read the gateway's local MPEG-TS
        // outputs, so their media timelines are identical and no skew
        // compensation is needed by default. Keep a tunable bias for setups
        // that bypass the gateway.
        int bias_ms = 0;
        if (const char* v = std::getenv("AIDETECT_ALARM_BIAS_MS"))
            bias_ms = std::max(0, std::atoi(v));
        auto r = recorder_->request_clip(
            alarm_ms - bias_ms, out, (pre_alarm_seconds_ + post + 20) * 1000);
        if (!r.ok) {
            log_warn(cfg_.task_id + " packet ring clip failed: " + r.error);
            return -1;
        }
        log_info(cfg_.task_id + " packet ring clip frames=" +
                 std::to_string(r.frames) + " duration_ms=" +
                 std::to_string(r.duration_ms) +
                 " keyframe=" + std::to_string(r.started_keyframe));
        std::error_code ec;
        if (!std::filesystem::exists(out) ||
            std::filesystem::file_size(out, ec) < 1024) {
            log_warn(cfg_.task_id + " packet ring produced no file: " + out);
            return -1;
        }
        return 0;
    }
    void save_alarm(ModelRuntime& rt, const std::vector<Detection>& dets, int count,
                    sail::BMImage& frame, sail::Bmcv& bmcv) {
        // Capture the trigger instant before image rendering/label HTTP: the
        // alarm image is this frame, so the video window must be centred here
        // (production's 50% backtracking ratio).
    // now_ms() is steady_clock (monotonic); alarm file names and the packet
    // ring window need wall-clock milliseconds.  Convert the frame instant:
    // wall_now - (steady_now - frame_steady) = the instant the frame arrived.
    const int64_t steady_now = now_ms();
    const int64_t frame_steady =
        current_frame_ms_.load() > 0 ? current_frame_ms_.load() : steady_now;
    const int64_t wall_now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const int64_t alarm_wall_ms = wall_now_ms - (steady_now - frame_steady);
        std::string model_name = rt.spec->name;
        std::string stamp = stamp_at_ms(alarm_wall_ms);
        std::string img_dir = app_.data_root + "/" + cfg_.task_id + "/" + model_name + "/img";
        std::string vid_dir = app_.data_root + "/" + cfg_.task_id + "/" + model_name + "/video";
        ensure_dir(img_dir);
        ensure_dir(vid_dir);
        std::string img_path = img_dir + "/" + stamp + ".jpg";
        std::string vid_path = vid_dir + "/" + stamp + ".mp4";
        alarm_render_ = true;
        alarm_image_path_ = img_path;
        // Alarm images follow the source resolution, not the preview cap:
        // production encodes the alarm frame itself (frameToJpegByCpu) at its
        // native size, and the alarm video is a -c copy of the source.
        Size2 alarm_sz = {frame_w_ > 0 ? frame_w_ : frame.width(),
                          frame_h_ > 0 ? frame_h_ : frame.height()};
        auto jpeg = render_jpeg(frame, bmcv, alarm_sz.w, alarm_sz.h, dets, img_path);
        alarm_render_ = false;
        std::vector<unsigned char> image_bytes = jpeg;
        {
            std::ifstream rf(img_path, std::ios::binary);
            if (rf) {
                std::vector<unsigned char> tmp((std::istreambuf_iterator<char>(rf)),
                                              std::istreambuf_iterator<char>());
                if (tmp.size() > 1024) image_bytes.swap(tmp);
            }
        }
        alarm_image_path_.clear();
        if (!image_bytes.empty() && !std::filesystem::exists(img_path)) {
            std::ofstream f(img_path, std::ios::binary);
            f.write(reinterpret_cast<const char*>(image_bytes.data()),
                    image_bytes.size());
            std::ofstream df("/tmp/alarm_jpeg.jpg", std::ios::binary);
            df.write(reinterpret_cast<const char*>(image_bytes.data()),
                     image_bytes.size());
        }
        Json report;
        report["task_id"] = cfg_.task_id;
        report["model_name"] = model_name;
        report["report_content"] = rt.spec->report_content;
        report["report_num"] = std::to_string(count);
        report["time"] = now_readable();
        report["image"] = img_path;
        report["video"] = "";
        log_warn(cfg_.task_id + " ALARM model=" + model_name + " count=" +
                 std::to_string(count) + " img=" + img_path);
        int post = std::max(1, cfg_.video_duration - 5);
        if (pending_.active) {
            // Finalize the previous clip instead of dropping this alarm's
            // video. Rapid multi-algorithm alarms used to get video="" here.
            finalize_video(true);
        }
        std::string tmp_dir = app_.tmp_root + "/" + cfg_.task_id;
        ensure_dir(tmp_dir);
        pending_.tmp_path = tmp_dir + "/" + stamp + ".mp4";
        pending_.final_path = vid_path;
        pending_.report = report;
        pending_.ready_ms = now_ms() + (post + 1) * 1000LL;
        int rc = -1;
        bool async_dump = false;
        // Preferred path: timestamped packet ring (libdump4-like). It keeps the
        // original H.264/H.265 packets and centres the clip on the alarm image.
        if (recorder_ && recorder_->running()) {
            rc = clip_recorder(alarm_wall_ms, post, pending_.tmp_path);
        }
        // Multi mode: MultiDecoder carries its own per-channel dump ring, so
        // the alarm clip is produced in-process (production has the same
        // in-process recorder).  No helper process and no HTTP video service.
        // In-process ES ring (production dump4 equivalent): the ingest bridge
        // keeps the last seconds of encoded packets and writes them to a TS
        // clip, so no URL re-open and no wall-clock <-> PTS mapping is needed.
        if (rc != 0 && seg_rec_.segment_count() > 0) {
            const std::string seg_ts = pending_.tmp_path + ".ts";
            size_t bytes = seg_rec_.clip(5, post, seg_ts);
            if (bytes > 0) {
                rc = 0;
                async_dump = true;
                log_info(cfg_.task_id + " segment clip started bytes=" +
                         std::to_string(bytes) + " segs=" +
                         std::to_string(seg_rec_.segment_count()));
            }
        }
        if (rc != 0 && use_multi_)
            rc = decode_pool_.dump(group_, slot_, 5, post, pending_.tmp_path);
        // The box runs with AIDETECT_NO_DUMP=1 to avoid SAIL dump crashes;
        // use the external video service when the packet ring could not
        // produce a file, otherwise HEVC/ISC tasks keep video_path empty.
        if (rc != 0 && no_dump_env()) {
            rc = dump_in_worker(upstream_url_.empty() ? source_url_ : upstream_url_,
                                5, post, pending_.tmp_path);
        }
        if (rc != 0 && !no_dump_env()) {
            if (use_multi_) {
                const char* vurl_env = std::getenv("AIDETECT_VIDEO_URL");
                std::string vurl = vurl_env ? vurl_env : "10.10.206.68:9071";
                size_t colon = vurl.rfind(':');
                if (colon != std::string::npos) {
                    Json req = {{"url", upstream_url_.empty() ? source_url_ : upstream_url_}, {"out", pending_.tmp_path},
                                {"pre", 5}, {"post", post}};
                    auto reply = http_post_raw(vurl.substr(0, colon),
                                               vurl.substr(colon + 1),
                                               "/dump", req.dump(), 30000);
                    try {
                        auto j = Json::parse(reply);
                        rc = j.value("rc", -1);
                    } catch (...) {
                        rc = -1;
                    }
                }
                if (rc != 0) {
                    rc = dump_in_worker(upstream_url_.empty() ? source_url_ : upstream_url_, 5, post, pending_.tmp_path);
                }
            } else if (decoder_) {
                rc = decoder_->dump(5, post, pending_.tmp_path);
                // sail::Decoder::dump() starts writing and needs the caller to
                // keep reading for the post seconds; finalize later, not now.
                async_dump = (rc == 0);
            }
        }
        log_info(cfg_.task_id + " alarm video rc=" + std::to_string(rc) +
                 " bridge=" + std::to_string(bridge_.running() ? 1 : 0) +
                 " ring=" +
                 std::to_string(recorder_ && recorder_->running() ? 1 : 0) +
                 " multi=" + std::to_string(use_multi_ ? 1 : 0) + " url=" +
                 (upstream_url_.empty() ? source_url_ : upstream_url_));
        if (rc != 0) {
            log_warn(cfg_.task_id + " alarm video failed rc=" + std::to_string(rc));
            pending_ = PendingVideo{};
            if (alarm_cb_) alarm_cb_(cfg_.task_id, report);
            return;
        }
        pending_.active = true;
        dumped_ = true;
        if (!async_dump) {
            // Ring clip / worker output is already complete: finalize now.
            finalize_video(true);
        } else {
            pending_.ready_ms = now_ms() + (post + 1) * 1000LL;
            log_info(cfg_.task_id + " decoder dump started, post=" +
                     std::to_string(post) + "s");
        }
    }
    static bool mp4_is_hevc(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        // The packet ring may put the moov box (and hvcC) at the end of the
        // file, so scan the whole clip; alarm clips are tens of MB at most.
        std::string s((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
        return s.find("hvcC") != std::string::npos ||
               s.find("hev1") != std::string::npos ||
               s.find("hvc1") != std::string::npos;
    }
    // Chrome/Edge cannot play HEVC in <video> on most Windows machines. The
    // packet ring copies the camera codec, so HEVC channels produced alarm
    // videos that downloaded fine but showed "format error" in the Web UI.
    // Re-encode only HEVC clips to H.264 with the on-board encoder.
    bool ensure_browser_mp4(const std::string& path) {
        if (!std::filesystem::exists(path) || !mp4_is_hevc(path)) return true;
        std::string tmp = path + ".h264.tmp.mp4";
        std::string log = "/tmp/" + cfg_.task_id + "-transcode.log";
        // Force the software HEVC decoder. With 30 concurrent stream decoders
        // the VPU has no free decode core, and letting ffmpeg pick a BK
        // decoder produced "VPU_DecIssueSeqInit"/empty output for 4K HEVC.
        // Software HEVC decode + h264_bm encode works on the same box.
        std::string cmd = "ffmpeg -y -nostdin -hide_banner -loglevel error -c:v hevc -i '" +
                          path + "' -c:v h264_bm -is_dma_buffer 0 -pix_fmt nv12 "
                          "-movflags +faststart -an '" + tmp + "' >" + log + " 2>&1";
        int rc = std::system(cmd.c_str());
        std::error_code ec;
        auto sz = std::filesystem::exists(tmp)
                      ? std::filesystem::file_size(tmp, ec)
                      : 0;
        if (rc == 0 && !ec && sz > 1024) {
            std::filesystem::rename(tmp, path, ec);
            if (!ec) {
                log_info(cfg_.task_id + " alarm video transcoded hevc->h264");
                return true;
            }
        }
        std::filesystem::remove(tmp, ec);
        log_warn(cfg_.task_id + " alarm video h264 transcode failed rc=" +
                 std::to_string(rc) + " " + tail_file(log, 400));
        return false;
    }
    void finalize_video(bool force) {
        if (!pending_.active) return;
        if (!force && now_ms() < pending_.ready_ms) return;
        bool ok = false;
        // The ring writer can publish the file just after request_clip()
        // returns on a busy box; retry briefly before declaring it missing.
        for (int i = 0; i < 30; ++i) {
            if (std::filesystem::exists(pending_.tmp_path) &&
                std::filesystem::file_size(pending_.tmp_path) > 1024)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        {
            const std::string ts_src = pending_.tmp_path + ".ts";
            if (!std::filesystem::exists(pending_.tmp_path) &&
                std::filesystem::exists(ts_src)) {
                // Wait for the ring writer to close the trailer, then remux to
                // MP4 (Annex-B TS -> MP4, stream copy, no re-encode).
                for (int i = 0; i < 60 && !bridge_.clip_done(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string cmd =
                    "ffmpeg -y -nostdin -hide_banner -loglevel error -i '" +
                    ts_src + "' -c copy -movflags +faststart '" +
                    pending_.tmp_path + "' >> /tmp/clip_remux.log 2>&1";
                int crc = std::system(cmd.c_str());
                std::error_code ecc;
                if (crc != 0) {
                    // Keep the TS for diagnosis instead of deleting evidence.
                    std::error_code kecc;
                    std::filesystem::rename(ts_src, ts_src + ".keep", kecc);
                    log_warn(cfg_.task_id + " clip remux failed rc=" +
                             std::to_string(crc) + " ts_kept=" + ts_src + ".keep");
                } else {
                    std::error_code kecc;
                    std::filesystem::remove(ts_src, kecc);
                }
            }
        }
        if (std::filesystem::exists(pending_.tmp_path) &&
            std::filesystem::file_size(pending_.tmp_path) > 1024) {
            // The packet ring already wrote a valid MP4; remuxing it with
            // ffmpeg -c copy can drop packets whose PTS/DTS were rebased, so
            // move/copy the bytes unchanged.
            std::error_code ec;
            std::filesystem::rename(pending_.tmp_path, pending_.final_path, ec);
            if (ec) {
                ec.clear();
                std::filesystem::copy_file(pending_.tmp_path, pending_.final_path,
                                           std::filesystem::copy_options::overwrite_existing,
                                           ec);
                if (!ec) std::filesystem::remove(pending_.tmp_path, ec);
            }
            ok = !ec && std::filesystem::exists(pending_.final_path) &&
                 std::filesystem::file_size(pending_.final_path) > 1024;
            if (!ok) log_warn(cfg_.task_id + " alarm clip move failed: " + ec.message());
        } else {
            log_warn(cfg_.task_id + " dump file missing: " + pending_.tmp_path);
        }
        if (ok) {
            ensure_browser_mp4(pending_.final_path);
            pending_.report["video"] = pending_.final_path;
        }
        if (alarm_cb_) alarm_cb_(cfg_.task_id, pending_.report);
        pending_ = PendingVideo{};
    }
    struct InferJob {
        std::shared_ptr<sail::BMImage> img;
        int64_t ts = 0;
    };
    TaskConfig cfg_;
    std::mutex pending_cfg_mu_;
    TaskConfig pending_cfg_;
    bool has_pending_cfg_ = false;
    mutable std::mutex models_mu_;
    std::mutex roi_mu_;
    int group_ = 0;
    int slot_ = 0;
    AppConfig app_;
    ModelScheduler& scheduler_;
    DecodePool& decode_pool_;
    bool use_multi_ = false;
    int open_fail_count_ = 0;
    // ISC stream preference: -1 = as configured, 0 = main, 1 = sub.  Used to
    // cut decode cost while nobody watches the channel (see want_single_).
    std::atomic<int> isc_stream_pref_{-1};
    SinglePreviewCallback single_cb_;
    PreviewCallback tile_cb_;
    DemandCallback want_single_;
    DemandCallback want_tile_;
    AlarmCallback alarm_cb_;
    bool no_preview_ = false;
    std::vector<std::unique_ptr<ModelRuntime>> models_;
    std::thread thread_;
    std::thread infer_thread_;
    std::mutex infer_mu_;
    std::condition_variable infer_cv_;
    std::deque<InferJob> infer_q_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> debug_alarm_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> failed_{false};
    std::string error_;
    std::mutex jpeg_mutex_;
    std::shared_ptr<std::vector<unsigned char>> latest_jpeg_;
    std::mutex area_jpeg_mutex_;
    std::shared_ptr<std::vector<unsigned char>> latest_area_jpeg_;
    std::atomic<int64_t> area_request_ms_{0};
    std::atomic<bool> force_area_{false};
    std::mutex det_mutex_;
    struct DetSnapshot {
        int64_t ts = 0;
        std::vector<Detection> dets;
        std::vector<Detection> persons;
    };
    std::vector<Detection> dets_;
    std::vector<Detection> dets_persons_;
    std::deque<DetSnapshot> dets_history_;
    int64_t dets_ms_ = 0;
    int64_t last_alarm_ms_ = 0;
    int frame_w_ = 0;
    int frame_h_ = 0;
    int preview_w_ = 0;   // 0 => derive from source, cap 1920x1080 (downscale only)
    int preview_h_ = 0;
    int tile_w_ = 640;
    int tile_h_ = 360;
    int preview_interval_ms_ = 100;
    double src_fps_ = 0;
    double tokens_ = 1.0;
    int64_t tile_interval_ms_ = 120;  // merge tile budget (8.3 Hz)
    double tile_tokens_ = 1.0;
    int64_t tile_last_token_ms_ = 0;
    int64_t last_token_ms_ = 0;
    int64_t last_area_ms_ = 0;
    std::string upstream_url_;
    std::string ring_url_;
    std::string gateway_pid_;
    pid_t gateway_pid_num_ = 0;
    int port_dec_ = 0;
    int port_rec_ = 0;
    std::string source_url_;
    std::string ring_dir_;
    std::unique_ptr<AlarmRingRecorder> recorder_;
    int pre_alarm_seconds_ = 5;
    std::thread preview_thread_;
    std::mutex preview_mu_;
    std::condition_variable preview_cv_;
    sail::BMImage preview_img_;         // pooled scaled preview (no per-tick alloc)
    int preview_img_w_ = 0;             // size of preview_img_
    int preview_img_h_ = 0;
    sail::BMImage* preview_frame_ptr_ = nullptr;  // borrowed under preview_mu_
    sail::BMImage track_img_;           // pooled resize target for preview tracking
    int track_img_w_ = 0;
    int track_img_h_ = 0;
    sail::BMImage tile_img_;            // pooled merge tile (640x360)
    int tile_img_w_ = 0;
    int tile_img_h_ = 0;
    std::vector<Detection> preview_dets_;
    std::vector<PreviewTrack> preview_tracks_;
    cv::Mat preview_prev_gray_;
    int64_t preview_prev_ts_ = 0;
    int64_t preview_last_det_ts_ = 0;
    int64_t preview_det_interval_ms_ = 0;
    bool preview_want_single_ = false;
    bool preview_want_tile_ = false;
    bool preview_has_work_ = false;
    std::atomic<bool> preview_busy_{false};
    // Inference demand controls (see constructor).
    int detect_min_interval_ms_ = 350;   // per-task detect cadence cap (2.9 Hz)
    size_t attr_models_per_round_ = 1000;  // 1000 = every crop algorithm each round
    size_t attr_turn_ = 0;               // round-robin cursor
    int max_crop_persons_ = 4;           // persons classified per round
    int64_t max_alg_interval_ms_ = 0;    // largest per-algorithm interval
    int64_t person_grace_ms_ = 900;      // keep a label while its algo skips
    int64_t last_pvprof_ms_ = 0;         // preview path profiling throttle
    int isc_rot_ = 0;                    // rotate ISC candidates after codec failures
    int upstream_hevc_ = -1;             // -1 unknown, 1 hevc, 0 other
    bool gateway_bsf_hevc_ = false;       // pass hevc_mp4toannexb to the gateway
    // In-process ingest (production keeps the pull/remux stage in its own
    // process): replaces the per-channel ffmpeg child with libavformat.
    IngestBridge bridge_;
    // Rolling TS segments from the bridge ring port: alarm clips are a byte
    // concatenation of the last seconds, no PTS/wall-clock mapping involved.
    TsSegmentRecorder seg_rec_;
    bool inproc_ingest_enabled() const {
        static const bool on = [] {
            const char* v = std::getenv("AIDETECT_INPROC_INGEST");
            // Runtime switch for A/B profiling: /tmp/aidetect_no_inproc falls
            // back to the external ffmpeg gateway without a restart.
            if (std::ifstream("/tmp/aidetect_no_inproc").good()) return false;
            return !v || std::string(v) != "0";
        }();
        return on;
    }
    int64_t target_ttl_ms_ = 2000;       // alarm target survives this gap
    std::string last_badge_label_;
    int64_t last_badge_ms_ = 0;
    // AIDETECT_PROFILE: periodic detection/preview counters used to attribute
    // multi-task lag (detect round time, snapshot age, drawn boxes).
    int64_t last_prof_ms_ = 0;
    std::map<std::string, cv::Mat> label_plates_;
    std::map<std::string, std::shared_ptr<sail::BMImage>> plate_cache_;
    // Normalized 0..1 ROI polygons from TaskAlgoConfigDetail (taskArea),
    // deduplicated across algorithms.  Production draws them as green lines
    // on single preview, merge tiles and alarm images.
    std::vector<std::vector<std::pair<float, float>>> roi_polygons_;
    std::mutex bar_bm_mu_;
    std::map<std::string, std::shared_ptr<sail::BMImage>> bar_bm_cache_;
    std::mutex status_text_mu_;
    std::string stream_status_ = "正在连接监控视频流...";
    bool rect_warned_ = false;
    bool plate_warned_ = false;
    std::shared_ptr<sail::Decoder> decoder_;
    bool dumped_ = false;
    int64_t last_status_ms_ = 0;
    std::atomic<int64_t> last_frame_ms_{0};
    std::atomic<int64_t> queued_frame_ms_{0};
    std::atomic<int64_t> current_frame_ms_{0};
    std::atomic<int64_t> started_ms_{0};
    std::atomic<int64_t> last_stall_restart_ms_{0};
    std::atomic<bool> high_res_held_{false};
    bool file_source_ = false;
    mutable std::mutex stream_err_mu_;
    std::string last_stream_error_;
    std::atomic<bool> ever_frame_{false};
    std::atomic<int64_t> last_progress_ms_{0};
    std::atomic<bool> need_recover_status_{false};
    std::atomic<bool> alarm_render_{false};
    std::atomic<int64_t> last_label_ms_{0};
    int64_t last_detect_ms_ = 0;
    std::string alarm_image_path_;
    std::thread status_thread_;
    PendingVideo pending_;
    cv::Ptr<cv::freetype::FreeType2> ft2_;
    std::atomic<bool> font_ok_{false};
};
}  // namespace aidetect
