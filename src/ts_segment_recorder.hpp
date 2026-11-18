#pragma once

// Rolling MPEG-TS segment recorder.
//
// The ingest bridge already fans the exact TS stream it feeds the decoder out to
// a second local UDP port.  This recorder drains that port and writes short
// segment files (default 5 s, keeping the last few).  An alarm clip is then just
// "concatenate the segments covering the last N seconds" - a pure byte copy,
// with no wall-clock <-> PTS mapping, no second pull and no ffmpeg child.
//
// Production does the same from its demuxer (libdump4); the difference is only
// that we slice on arrival time and keep raw TS files.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aidetect {

class TsSegmentRecorder {
public:
    TsSegmentRecorder() = default;
    ~TsSegmentRecorder() { stop(); }

    TsSegmentRecorder(const TsSegmentRecorder&) = delete;
    TsSegmentRecorder& operator=(const TsSegmentRecorder&) = delete;

    bool start(int udp_port, std::string dir, int seg_seconds = 5, int keep = 8) {
        stop();
        port_ = udp_port;
        dir_ = std::move(dir);
        seg_seconds_ = seg_seconds > 0 ? seg_seconds : 5;
        keep_ = keep > 1 ? keep : 8;
        // Seed the in-memory index with segments already on disk (after a
        // process restart the directory may still hold a usable window).
        segments_.clear();
        if (std::filesystem::exists(dir_)) {
            for (auto& e : std::filesystem::directory_iterator(dir_)) {
                if (!e.is_regular_file()) continue;
                auto name = e.path().filename().string();
                if (name.rfind("seg_", 0) != 0) continue;
                auto t = e.last_write_time().time_since_epoch();
                int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
                segments_.push_back({e.path().string(), ms});
            }
            std::sort(segments_.begin(), segments_.end(),
                      [](const Seg& a, const Seg& b) { return a.until_ms < b.until_ms; });
        }
        running_ = true;
        thread_ = std::thread([this] { loop(); });
        return true;
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
        std::lock_guard<std::mutex> lk(mu_);
        if (file_.is_open()) file_.close();
    }

    bool running() const { return running_.load(); }

    // Concatenate the segments that cover [now-pre, now+post] into out_path.
    // Returns bytes written (0 means no data was available).
    size_t clip(int pre_s, int post_s, const std::string& out_path) {
        std::lock_guard<std::mutex> lk(mu_);
        if (file_.is_open()) file_.flush();
        const int64_t now = now_ms();
        const int64_t from = now - static_cast<int64_t>(std::max(1, pre_s) + std::max(0, post_s)) * 1000;
        std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return 0;
        size_t total = 0;
        for (auto& s : segments_) {
            if (s.until_ms < from) continue;
            std::ifstream in(s.path, std::ios::binary);
            if (!in.is_open()) continue;
            out << in.rdbuf();
            total += static_cast<size_t>(std::filesystem::file_size(s.path));
        }
        // Include whatever is in the file being written right now.
        if (file_.is_open()) {
            std::string cur = current_path_;
            if (!cur.empty()) {
                std::ifstream in(cur, std::ios::binary);
                if (in.is_open()) {
                    out << in.rdbuf();
                    total += static_cast<size_t>(std::filesystem::file_size(cur));
                }
            }
        }
        out.close();
        (void)post_s;
        return total;
    }

    size_t segment_count() {
        std::lock_guard<std::mutex> lk(mu_);
        return segments_.size();
    }

private:
    struct Seg {
        std::string path;
        int64_t until_ms = 0;
    };

    static int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    void rotate_locked(int64_t now) {
        if (file_.is_open()) {
            file_.close();
            segments_.push_back({current_path_, now});
            while (static_cast<int>(segments_.size()) > keep_) {
                std::remove(segments_.front().path.c_str());
                segments_.erase(segments_.begin());
            }
        }
        current_path_ = dir_ + "/seg_" + std::to_string(now) + ".ts";
        file_.open(current_path_, std::ios::binary | std::ios::trunc);
        seg_started_ms_ = now;
    }

    void loop() {
        int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            running_ = false;
            return;
        }
        int rcvbuf = 4 * 1024 * 1024;
        ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port_));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(sock);
            running_ = false;
            return;
        }
        timeval tv{0, 200000};
        ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        std::vector<uint8_t> buf(65536);
        while (running_) {
            ssize_t n = ::recv(sock, buf.data(), buf.size(), 0);
            const int64_t now = now_ms();
            std::lock_guard<std::mutex> lk(mu_);
            if (!file_.is_open() ||
                now - seg_started_ms_ >= static_cast<int64_t>(seg_seconds_) * 1000)
                rotate_locked(now);
            if (n > 0 && file_.is_open()) {
                file_.write(reinterpret_cast<const char*>(buf.data()), n);
                file_.flush();
            }
        }
        ::close(sock);
        std::lock_guard<std::mutex> lk(mu_);
        if (file_.is_open()) file_.close();
    }

    int port_ = 0;
    std::string dir_;
    int seg_seconds_ = 5;
    int keep_ = 8;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mu_;
    std::ofstream file_;
    std::string current_path_;
    int64_t seg_started_ms_ = 0;
    std::vector<Seg> segments_;
};

}  // namespace aidetect
