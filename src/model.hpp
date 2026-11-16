#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <fstream>
#include <dlfcn.h>

#include "common.hpp"
#include <sail/cvwrapper.h>
#include <sail/engine.h>
#include <sail/tensor.h>

namespace aidetect {

inline int shape_count(const std::vector<int>& s) {
    int n = 1;
    for (int v : s) n *= v;
    return n;
}

enum class ModelType { Yolo11, Damo, YoloV5, YoloV8, YoloV10, YoloV10Crop, YoloV5Crop, AttrClassifier, CrowdHead, BoatLifeJacket };

struct ModelSpec {
    std::string name;
    std::string path;
    ModelType type = ModelType::Damo;
    std::vector<std::string> class_names;
    int target_id = 0;
    std::string report_content;
    int default_exceed = 1;
    float alpha = 1.0f;  // convert_to alpha
    float beta = 0.0f;   // convert_to beta
    int input_w = 640;
    int input_h = 640;
    bool class_logits = false;  // YoloV5: class columns are logits
    int attr_head = 0;          // AttrClassifier: which output head
    int attr_class = 0;         // AttrClassifier: violation class
    float attr_thresh = 0.5f;   // AttrClassifier: alarm threshold
    int target_id2 = -1;        // optional second target class
};

struct ModelConfig;

// Production plugin loader: each libalg_plugin_*.so embeds its bmodel and
// exports get_plugin()/get_plugin_size(). Extract the bmodel once into
// models/plugins/<name>.bmodel so the rest of the app can use it as a normal
// ModelSpec. This is the "drop a plugin .so and it becomes an algorithm" path.
inline void ensure_plugin_models(const std::string& root) {
    static const char* names[] = {"head", "people", "people_big", "behavior",
                                  "helmet", "crowd", "suit"};
    for (const char* n : names) {
        std::string so = root + "/plugins/libalg_plugin_" + n + ".so";
        std::string bm = root + "/plugins/" + std::string(n) + ".bmodel";
        std::ifstream test(bm, std::ios::binary);
        if (test.good()) continue;
        void* h = dlopen(so.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!h) continue;
        auto gp = reinterpret_cast<void* (*)()>(dlsym(h, "get_plugin"));
        auto gs = reinterpret_cast<size_t (*)()>(dlsym(h, "get_plugin_size"));
        if (gp && gs) {
            void* p = gp();
            size_t sz = gs();
            if (p && sz > 0) {
                std::ofstream out(bm, std::ios::binary);
                out.write(static_cast<const char*>(p), static_cast<std::streamsize>(sz));
                out.close();
                log_info("plugin model extracted: " + std::string(n) + " " +
                         std::to_string(sz));
            }
        }
        dlclose(h);
    }
}

inline std::vector<ModelSpec> make_model_specs(const std::string& root) {
    ensure_plugin_models(root);
    std::vector<ModelSpec> v;
    v.push_back({"reflectivejacket",
                 root + "/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel",
                 ModelType::Yolo11,
                 {"穿反光衣", "未穿反光衣"},
                 1,
                 "未穿反光衣",
                 1,
                 1.0f / 255.0f,
                 0.0f});
    v.push_back({"reflectivejacket_sky",
                 root + "/damoyolo_tinynasL25_S_fp16_reflectivejacket_sky_1.bmodel",
                 ModelType::Damo,
                 {"穿反光衣", "未穿反光衣"},
                 1,
                 "未穿反光衣",
                 1,
                 1.0f,
                 1.0f});
    v.push_back({"safetyhelmet",
                 root + "/damoyolo_tinynasL25_S_safetyhelmet.bmodel",
                 ModelType::Damo,
                 {"戴安全帽", "未戴安全帽"},
                 1,
                 "未戴安全帽",
                 1,
                 1.0f,
                 1.0f});
    v.push_back({"lifejacket",
                 root + "/damoyolo_tinynasL25_S_lifejacket_2.bmodel",
                 ModelType::Damo,
                 {"穿救生衣", "未穿救生衣"},
                 1,
                 "未穿救生衣",
                 1,
                 1.0f,
                 1.0f});
    v.push_back({"cigarette",
                 root + "/damoyolo_tinynasL25_S_cigarette.bmodel",
                 ModelType::Damo,
                 {"抽烟行为"},
                 0,
                 "抽烟行为",
                 1,
                 1.0f,
                 1.0f});
    v.push_back({"person",
                 root + "/damoyolo_tinynasL25_S_person.bmodel",
                 ModelType::Damo,
                 {"人"},
                 0,
                 "人员聚集",
                 3,
                 1.0f,
                 1.0f});
    // Production plugin models (embedded bmodel extracted from libalg_plugin_*.so).
    v.push_back({"head",
                 root + "/plugins/head.bmodel",
                 ModelType::YoloV5,
                 {"Head"},
                 0,
                 "人数监控",
                 1,
                 1.0f / 255.0f,
                 0.0f,
                 832,
                 480,
                 false});
    v.push_back({"people",
                 root + "/plugins/people.bmodel",
                 ModelType::YoloV5,
                 {"People", "Class1", "Class2", "Class3"},
                 0,
                 "人员聚集",
                 3,
                 1.0f / 255.0f,
                 0.0f,
                 640,
                 384,
                 true});
    // Production "people_big" plugin (enable_people_deep_mode): input 832x480,
    // output [1,8190,9]. This is the real large-resolution people detector,
    // not a 2x2 tiling hack.
    v.push_back({"people_big",
                 root + "/plugins/people_big.bmodel",
                 ModelType::YoloV5,
                 {"People", "Class1", "Class2", "Class3"},
                 0,
                 "人员聚集",
                 3,
                 1.0f / 255.0f,
                 0.0f,
                 832,
                 480,
                 true});
    v.push_back({"behavior",
                 root + "/plugins/behavior.bmodel",
                 ModelType::YoloV5,
                 {"吸烟", "打电话", "玩手机", "正常", "其他"},
                 0,
                 "抽烟打电话",
                 1,
                 1.0f,
                 0.0f,
                 256,
                 256,
                 true});    v.push_back({"helmet",
                 root + "/plugins/helmet.bmodel",
                 ModelType::AttrClassifier,
                 {"未戴安全帽"},
                 0,
                 "未戴安全帽",
                 1,
                 1.0f,
                 0.0f,
                 224,
                 224,
                 false,
                 1,
                 0,
                 0.8f});
    v.push_back({"vest",
                 root + "/plugins/helmet.bmodel",
                 ModelType::AttrClassifier,
                 {"未穿反光衣"},
                 0,
                 "未穿反光衣",
                 1,
                 1.0f,
                 0.0f,
                 224,
                 224,
                 false,
                 0,
                 0,
                 0.9f});    v.push_back({"smoking_call",
                 root + "/plugins/behavior.bmodel",
                 ModelType::YoloV5Crop,
                 {"吸烟", "打电话", "玩手机", "正常", "其他"},
                 2,
                 "抽烟/打电话",
                 1,
                 1.0f,
                 0.0f,
                 256,
                 256,
                 true,
                 0,
                 0,
                 0.4f,
                 3});    v.push_back({"people_gather",
                 root + "/plugins/people.bmodel",
                 ModelType::YoloV5,
                 {"People", "Class1", "Class2", "Class3"},
                 0,
                 "人员聚集",
                 3,
                 1.0f,
                 0.0f,
                 640,
                 384,
                 true});
    v.push_back({"head_count",
                 root + "/plugins/head.bmodel",
                 ModelType::YoloV5,
                 {"Head"},
                 0,
                 "人数监控",
                 1,
                 1.0f,
                 0.0f,
                 832,
                 480,
                 false});    // Legacy algorithms exposed as *_our in the UI. They reuse the old Damo
    // models and are selectable through the same plugin/config mechanism.
    auto alias = [&](const char* name, const char* base) {
        for (auto& s : v) {
            if (s.name == base) {
                ModelSpec a = s;
                a.name = name;
                v.push_back(a);
                break;
            }
        }
    };
    alias("person_our", "person");
    alias("reflectivejacket_our", "reflectivejacket");
    alias("reflectivejacket_sky_our", "reflectivejacket_sky");
    alias("safetyhelmet_our", "safetyhelmet");
    alias("cigarette_our", "cigarette");
    alias("lifejacket_our", "lifejacket");    v.push_back({"boat_life_jacket",
                 root + "/plugins/boat_life_jacket.bmodel",
                 ModelType::BoatLifeJacket,
                 {"救生衣1", "人员1", "救生衣2", "未穿救生衣", "救生衣3", "人员2"},
                 3,
                 "未穿救生衣",
                 1,
                 1.0f,
                 0.0f,
                 256,
                 256,
                 true});    v.push_back({"crowd_head",
                 root + "/plugins/crowd.bmodel",
                 ModelType::CrowdHead,
                 {"背景", "人头"},
                 1,
                 "人员聚集",
                 1,
                 0.0171f,
                 0.0f,
                 640,
                 384,
                 true});    v.push_back({"helmet_v3",
                 root + "/plugins/minor_helmet.bmodel",
                 ModelType::YoloV10Crop,
                 // Calibrated on production NoHelmet/helmet samples:
                 // class 0 = 未戴安全帽, class 1 = 安全帽, class 2 = 人员.
                 {"未戴安全帽", "安全帽", "人员"},
                 0,
                 "未戴安全帽",
                 1,
                 1.0f,
                 0.0f,
                 256,
                 256,
                 true});    return v;
}

inline const ModelSpec* find_model_spec(const std::vector<ModelSpec>& specs,
                                        const std::string& name) {
    for (auto& s : specs)
        if (s.name == name) return &s;
    return nullptr;
}

struct TensorData {
    std::string name;
    std::vector<int> shape;
    std::vector<float> data;
};

struct InferOutput {
    std::vector<TensorData> tensors;
};

// One sail::Engine. A single Engine must not be called concurrently; the
// scheduler guarantees that. Models are loaded lazily by ModelScheduler.
class ModelInstance {
public:
    explicit ModelInstance(const ModelSpec& spec) : spec_(spec) {
        engine_ = std::make_unique<sail::Engine>(spec.path, 0, sail::IOMode::SYSIO);
        graphs_ = engine_->get_graph_names();
        if (graphs_.empty()) throw std::runtime_error("model has no graph: " + spec.path);
        graph_ = graphs_[0];
        auto inputs = engine_->get_input_names(graph_);
        if (inputs.empty()) throw std::runtime_error("model has no input: " + spec.path);
        input_name_ = inputs[0];
        input_shape_ = engine_->get_input_shape(graph_, input_name_);
        input_dtype_ = engine_->get_input_dtype(graph_, input_name_);
        input_shapes_[input_name_] = input_shape_;
        for (auto& name : engine_->get_output_names(graph_)) {
            OutputInfo oi;
            oi.name = name;
            oi.shape = engine_->get_output_shape(graph_, name);
            oi.dtype = engine_->get_output_dtype(graph_, name);
            outputs_.push_back(oi);
        }
        log_info("model loaded: " + spec.name + " " + spec.path + " input=" +
                 shape_to_string(input_shape_) + " outputs=" +
                 std::to_string(outputs_.size()));
    }

    const ModelSpec& spec() const { return spec_; }
    const std::vector<int>& input_shape() const { return input_shape_; }

    std::atomic<int>& busy() { return busy_; }
    void touch() { last_used_ms_.store(now_ms()); }
    int64_t idle_ms() { return now_ms() - last_used_ms_.load(); }

    InferOutput infer(sail::BMImage& input_img, sail::Handle& handle,
                      sail::Bmcv& bmcv) {
        std::lock_guard<std::mutex> lk(mutex_);
        sail::Tensor input(handle, input_shape_, input_dtype_, true, true);
        bmcv.bm_image_to_tensor(input_img, input);
        std::map<std::string, sail::Tensor*> input_map{{input_name_, &input}};
        std::vector<std::unique_ptr<sail::Tensor>> output_storage;
        std::map<std::string, sail::Tensor*> output_map;
        for (auto& oi : outputs_) {
            auto t = std::make_unique<sail::Tensor>(handle, oi.shape, oi.dtype, true, true);
            output_map[oi.name] = t.get();
            output_storage.push_back(std::move(t));
        }
        engine_->process(graph_, input_map, input_shapes_, output_map);
        InferOutput result;
        for (size_t i = 0; i < outputs_.size(); ++i) {
            auto& t = *output_storage[i];
            t.sync_d2s();
            TensorData td;
            td.name = outputs_[i].name;
            td.shape = outputs_[i].shape;
            int count = shape_count(td.shape);
            td.data.resize(count);
            const float* p = static_cast<const float*>(t.sys_data());
            if (p) std::copy(p, p + count, td.data.begin());
            result.tensors.push_back(std::move(td));
        }
        return result;
    }

    static std::string shape_to_string(const std::vector<int>& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (i) out += ",";
            out += std::to_string(s[i]);
        }
        return out;
    }
private:
    struct OutputInfo {
        std::string name;
        std::vector<int> shape;
        bm_data_type_t dtype = BM_FLOAT32;
    };
    ModelSpec spec_;
    std::unique_ptr<sail::Engine> engine_;
    std::vector<std::string> graphs_;
    std::string graph_;
    std::string input_name_;
    std::vector<int> input_shape_;
    bm_data_type_t input_dtype_ = BM_FLOAT32;
    std::vector<OutputInfo> outputs_;
    std::map<std::string, std::vector<int>> input_shapes_;
    std::mutex mutex_;
    std::atomic<int> busy_{0};
    std::atomic<int64_t> last_used_ms_{now_ms()};
};

// Multi-instance scheduler. Same model shares instances; if all instances are
// busy and max_instances is not reached, a second Engine is loaded lazily.
class ModelScheduler {
public:
    explicit ModelScheduler(const AppConfig& cfg)
        : specs_(make_model_specs(cfg.model_root)) {
        const char* cc = std::getenv("AIDETECT_INFER_CONCURRENCY");
        concurrency_ = cc ? std::max(1, std::atoi(cc)) : 4;
        const char* e = std::getenv("AIDETECT_MAX_INSTANCES");
        // Enough engine instances that concurrency_ different tasks using the
        // same algorithm do not queue behind one engine mutex.
        max_per_model_ = e ? std::max(1, std::atoi(e))
                           : std::max(1, concurrency_ / 2);
        const char* idle = std::getenv("AIDETECT_MODEL_IDLE_SECONDS");
        idle_seconds_ = idle ? std::max(1, std::atoi(idle)) : 120;
        log_info("infer scheduler concurrency=" + std::to_string(concurrency_) +
                 " max_instances/model=" + std::to_string(max_per_model_));
        janitor_ = std::thread([this] { janitor_loop(); });
    }
    ~ModelScheduler() {
        stop_janitor_ = true;
        if (janitor_.joinable()) janitor_.join();
    }

    const std::vector<ModelSpec>& specs() const { return specs_; }
    const ModelSpec* spec(const std::string& name) const {
        return find_model_spec(specs_, name);
    }

    int instance_count(const std::string& name) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = instances_.find(name);
        return it == instances_.end() ? 0 : static_cast<int>(it->second.size());
    }

    Json stats() {
        std::lock_guard<std::mutex> lk(mu_);
        Json out = Json::array();
        for (auto& kv : instances_) {
            for (auto& p : kv.second) {
                out.push_back({{"model", kv.first},
                               {"busy", p->busy().load()},
                               {"idle_ms", p->idle_ms()}});
            }
        }
        return out;
    }

    InferOutput infer(const ModelSpec& spec, sail::BMImage& input_img,
                      sail::Handle& handle, sail::Bmcv& bmcv) {
        // Production runs every algorithm as its own pipeline and never puts a
        // single global lock in front of all TPU work: `ImageTaskInstance::
        // workLoop` submits `sdkImageInference(...)` with a completion callback
        // (`onInferenceFinished`) and skips algorithms for frames it cannot
        // keep up with (`TaskInstance::onAllAlgoSkipped`).  A plain global
        // mutex here meant a 15-task box served whichever task re-acquired the
        // lock first; test_2 (4K, five algorithms) starved to one detection
        // every ~3s while empty scenes got five rounds per second.
        //
        // Replacement: a FIFO ticket gate with a bounded number of concurrent
        // inferences.  Admission is in arrival order, so a task that submits
        // one request at a time (every Task has a single infer thread) gets a
        // fair 1/N share, and up to `concurrency_` engines can run in
        // parallel instead of one.
        const int64_t t0 = now_ms();
        const int cur = inflight_.fetch_add(1) + 1;
        int peak = max_inflight_.load();
        while (cur > peak && !max_inflight_.compare_exchange_weak(peak, cur)) {
        }
        const uint64_t ticket = enter_queue();
        const int64_t admitted_at = now_ms();
        InferOutput out;
        try {
            auto inst = acquire(spec);
            try {
                out = inst->infer(input_img, handle, bmcv);
                release(inst);
            } catch (...) {
                release(inst);
                throw;
            }
        } catch (...) {
            leave_queue();
            inflight_.fetch_sub(1);
            throw;
        }
        leave_queue();
        inflight_.fetch_sub(1);
        if (profiling()) record(spec.name, t0, admitted_at);
        return out;
    }

    // Queue depth relative to the admission limit (public: tasks slow their
    // own detection cadence when the shared pipeline is behind, which is the
    // intent of production TaskInstance::onAllAlgoSkipped).
    double load() {
        std::lock_guard<std::mutex> lk(q_mu_);
        return static_cast<double>(active_ + static_cast<int>(q_.size())) /
               std::max(1, concurrency_);
    }
private:
    // FIFO admission: tickets are served oldest-first; at most concurrency_
    // requests run at once.  This is the fairness point production gets from
    // giving every task instance its own asynchronous work loop.
    uint64_t enter_queue() {
        std::unique_lock<std::mutex> lk(q_mu_);
        const uint64_t ticket = next_ticket_++;
        q_.push_back(ticket);
        q_cv_.wait(lk, [&] {
            return q_.front() == ticket && active_ < concurrency_;
        });
        q_.pop_front();
        ++active_;
        return ticket;
    }
    void leave_queue() {
        {
            std::lock_guard<std::mutex> lk(q_mu_);
            --active_;
        }
        q_cv_.notify_all();
    }
    std::shared_ptr<ModelInstance> acquire(const ModelSpec& spec) {
        std::lock_guard<std::mutex> lk(mu_);
        auto& list = instances_[spec.name];
        if (list.empty()) {
            list.push_back(std::make_shared<ModelInstance>(spec));
        }
        std::shared_ptr<ModelInstance> best = list.front();
        for (auto& p : list) {
            if (p->busy().load() < best->busy().load()) best = p;
        }
        if (best->busy().load() > 0 &&
            static_cast<int>(list.size()) < max_per_model_) {
            list.push_back(std::make_shared<ModelInstance>(spec));
            best = list.back();
        }
        best->busy().fetch_add(1);
        best->touch();
        return best;
    }

    void release(const std::shared_ptr<ModelInstance>& inst) {
        inst->busy().fetch_sub(1);
        inst->touch();
    }

    void janitor_loop() {
        while (!stop_janitor_) {
            for (int i = 0; i < 50 && !stop_janitor_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (stop_janitor_) break;
            if (profiling()) log_stats();
            std::vector<std::shared_ptr<ModelInstance>> retired;
            {
                std::lock_guard<std::mutex> lk(mu_);
                int64_t ttl_ms = static_cast<int64_t>(idle_seconds_) * 1000;
                for (auto& kv : instances_) {
                    auto& list = kv.second;
                    if (list.size() <= 1) continue;
                    for (auto it = list.begin(); it != list.end() && list.size() > 1;) {
                        if ((*it)->busy().load() == 0 && (*it)->idle_ms() >= ttl_ms) {
                            log_info("model instance reclaimed: " + kv.first);
                            retired.push_back(*it);
                            it = list.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
            // Engine destruction happens outside mu_; may be slow.
            retired.clear();
        }
    }

    std::vector<ModelSpec> specs_;
    std::mutex mu_;
    // FIFO admission queue (see enter_queue).  concurrency_ bounds how many
    // engines may run at the same time; 1 reproduces the old serialized
    // behaviour, 4 lets the four TPU clusters work in parallel.
    std::mutex q_mu_;
    std::condition_variable q_cv_;
    std::deque<uint64_t> q_;
    uint64_t next_ticket_ = 0;
    int active_ = 0;
    int concurrency_ = 4;
    // AIDETECT_PROFILE: log where inference time actually goes (queue wait vs
    // TPU work) per model, plus the peak concurrency, so multi-task lag can be
    // attributed to serialization instead of guessed at.
    std::mutex stat_mu_;
    struct InferStat {
        int64_t calls = 0;
        int64_t total_ms = 0;
        int64_t wait_ms = 0;
        int64_t max_total_ms = 0;
        int64_t max_wait_ms = 0;
    };
    std::map<std::string, InferStat> stats_;
    std::atomic<int> inflight_{0};
    std::atomic<int> max_inflight_{0};
    void record(const std::string& name, int64_t t0, int64_t locked_at) {
        const int64_t done = now_ms();
        std::lock_guard<std::mutex> lk(stat_mu_);
        auto& s = stats_[name];
        s.calls++;
        int64_t wait = locked_at > t0 ? locked_at - t0 : 0;
        int64_t total = done - t0;
        s.wait_ms += wait;
        s.total_ms += total;
        s.max_wait_ms = std::max(s.max_wait_ms, wait);
        s.max_total_ms = std::max(s.max_total_ms, total);
    }
    void log_stats() {
        std::map<std::string, InferStat> snap;
        {
            std::lock_guard<std::mutex> lk(stat_mu_);
            snap.swap(stats_);
        }
        std::string line = "infer stats 5s peak_pending=" +
                           std::to_string(max_inflight_.load());
        max_inflight_.store(inflight_.load());
        for (auto& kv : snap) {
            auto& s = kv.second;
            int64_t n = std::max<int64_t>(1, s.calls);
            line += " | " + kv.first + " n=" + std::to_string(s.calls) +
                    " avg=" + std::to_string(s.total_ms / n) +
                    " wait_avg=" + std::to_string(s.wait_ms / n) +
                    " max=" + std::to_string(s.max_total_ms) +
                    " wait_max=" + std::to_string(s.max_wait_ms);
        }
        log_info(line);
    }
    std::map<std::string, std::vector<std::shared_ptr<ModelInstance>>> instances_;
    int max_per_model_ = 2;
    int idle_seconds_ = 120;
    std::atomic<bool> stop_janitor_{false};
    std::thread janitor_;
};

}  // namespace aidetect
