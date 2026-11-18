#pragma once

// Persistent per-(task, algorithm) last-report time.
//
// Production keeps this in edge.db `table_alarm_interval(id, time_ms)` and
// `AlarmRecorder::getLastALarmTime/setLastALarmTime`, so a service restart does
// not immediately fire the 3-minute / 86400-second report gate again for a
// target that was already reported. Ours used to live only in memory
// (rt.next_report_ms), so the first frame after every restart could re-report
// the same event. This header closes that gap with a small JSON file under the
// mounted data root (survives container restarts).

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>

#include "common.hpp"

namespace aidetect {

class AlarmStateStore {
public:
    int64_t last(const std::string& task_id, const std::string& model) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = map_.find(key(task_id, model));
        return it == map_.end() ? 0 : it->second;
    }

    void set(const std::string& task_id, const std::string& model, int64_t ms) {
        std::lock_guard<std::mutex> lk(mu_);
        map_[key(task_id, model)] = ms;
        dirty_ = true;
        if (now_ms() - last_save_ms_ > 5000) save_locked();
    }

    // Called periodically from the supervisor loop; also flushes at most every
    // 5 s on writes.
    void flush() {
        std::lock_guard<std::mutex> lk(mu_);
        if (dirty_) save_locked();
    }

private:
    static std::string key(const std::string& t, const std::string& m) {
        return t + "|" + m;
    }
    static std::string path() {
        const char* d = std::getenv("AIDETECT_DATA_ROOT");
        std::string root = d && *d ? d : "/data/CT/project/ai/inference/save";
        return root + "/.state/alarm_intervals.json";
    }
    void load() {
        std::ifstream f(path());
        if (!f) return;
        try {
            Json j = Json::parse(f);
            for (auto it = j.begin(); it != j.end(); ++it)
                if (it.value().is_number()) map_[it.key()] = it.value().get<int64_t>();
            log_info("alarm interval state loaded: " + std::to_string(map_.size()) + " entries");
        } catch (...) {
            log_warn("alarm interval state parse failed; starting empty");
        }
    }
    // Caller holds mu_.
    void save_locked() {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path()).parent_path(), ec);
        Json j = Json::object();
        for (auto& kv : map_) j[kv.first] = kv.second;
        const std::string tmp = path() + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) return;
            f << j.dump();
        }
        std::filesystem::rename(tmp, path(), ec);
        if (!ec) { dirty_ = false; last_save_ms_ = now_ms(); }
    }

    std::mutex mu_;
    std::map<std::string, int64_t> map_;
    bool dirty_ = false;
    int64_t last_save_ms_ = 0;
    bool loaded_ = false;

public:
    AlarmStateStore() { load(); loaded_ = true; }
};

inline AlarmStateStore& alarm_state() {
    static AlarmStateStore s;
    return s;
}

}  // namespace aidetect
