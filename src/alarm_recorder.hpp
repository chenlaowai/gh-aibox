#pragma once

// Timestamped encoded-packet ring for alarm evidence.
//
// Production uses libdump4 (FrameBuffering + dump_task + Mp4V2Writer): one
// network stream per channel, a pre-roll buffer of encoded packets, and a
// remux to MP4 on alarm.  sail::Decoder::dump() only keeps a short GOP and was
// measured 2s after the alarm image, so we keep our own packet ring:
//
//   - read packets from the source (no decode, no transcode)
//   - map packet DTS to wall clock from the first packet of the connection
//   - keep the last pre+margin seconds
//   - on alarm, wait until the ring covers alarm+post, then remux
//     [alarm-pre, alarm+post] to MP4 with the original H.264/H.265 stream
//
// Resolution and frame rate are preserved because packets are copied.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
}

#include "common.hpp"

namespace aidetect {

class AlarmRingRecorder {
public:
    AlarmRingRecorder() = default;
    ~AlarmRingRecorder() { stop(); }
    AlarmRingRecorder(const AlarmRingRecorder&) = delete;
    AlarmRingRecorder& operator=(const AlarmRingRecorder&) = delete;

    void start(const std::string& url, int pre_seconds, int post_seconds,
               int ring_margin_seconds = 5, int max_ring_mb = 32) {
        url_ = url;
        pre_ = std::max(1, pre_seconds);
        post_ = std::max(1, post_seconds);
        margin_ = std::max(1, ring_margin_seconds);
        max_bytes_ = static_cast<int64_t>(std::max(8, max_ring_mb)) * 1024 * 1024;
        stop_ = false;
        thread_ = std::thread([this] { loop(); });
    }

    void stop() {
        stop_ = true;
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

    bool running() const { return running_; }

    struct ClipResult {
        bool ok = false;
        std::string error;
        int64_t duration_ms = 0;
        int frames = 0;
        int started_keyframe = 0;
    };

    // Blocks until the post-roll is available and the MP4 is written.
    ClipResult request_clip(int64_t alarm_wall_ms, const std::string& out,
                            int timeout_ms = 30000) {
        ClipResult res;
        std::unique_lock<std::mutex> lk(req_mu_);
        if (!running_) {
            res.error = "recorder not running";
            return res;
        }
        req_ = Req{};
        req_.active = true;
        req_.alarm_ms = alarm_wall_ms;
        req_.out = out;
        req_.deadline_ms = alarm_wall_ms + 1000LL * (post_ + 12);
        req_.hard_deadline_ms = wall_now_ms() + timeout_ms;
        cv_.notify_all();
        cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                     [this] { return req_.done; });
        if (!req_.done) {
            res.error = "clip timeout";
            req_.active = false;
            return res;
        }
        res.ok = req_.ok;
        res.error = req_.error;
        res.duration_ms = req_.duration_ms;
        res.frames = req_.frames;
        res.started_keyframe = req_.started_keyframe;
        req_.active = false;
        return res;
    }

private:
    struct Pkt {
        std::vector<uint8_t> data;
        int64_t dts = AV_NOPTS_VALUE;
        int64_t pts = AV_NOPTS_VALUE;
        int64_t wall_ms = 0;
        bool key = false;
    };
    struct Req {
        bool active = false;
        bool done = false;
        bool ok = false;
        int64_t alarm_ms = 0;
        int64_t deadline_ms = 0;
        int64_t hard_deadline_ms = 0;
        std::string out;
        std::string error;
        int64_t duration_ms = 0;
        int frames = 0;
        int started_keyframe = 0;
    };
    struct Input {
        AVFormatContext* fmt = nullptr;
        AVStream* st = nullptr;
        int stream_index = -1;
        AVRational tb{1, 1000};
        bool ok = false;
    };

    // Wall-clock milliseconds: alarm file names and the clip window passed by
    // Task::save_alarm are system_clock epoch values, so the ring timeline
    // must use the same clock or the requested window never matches.
    static int64_t wall_now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    bool open_input(Input& in) {
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "stimeout", "5000000", 0);
        av_dict_set(&opts, "rw_timeout", "5000000", 0);
        AVFormatContext* fmt = nullptr;
        int ret = avformat_open_input(&fmt, url_.c_str(), nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0 || !fmt) return false;
        if (avformat_find_stream_info(fmt, nullptr) < 0) {
            avformat_close_input(&fmt);
            return false;
        }
        int idx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (idx < 0) {
            avformat_close_input(&fmt);
            return false;
        }
        in.fmt = fmt;
        in.st = fmt->streams[idx];
        in.stream_index = idx;
        in.tb = in.st->time_base;
        in.ok = true;
        return true;
    }

    static void close_input(Input& in) {
        if (in.fmt) avformat_close_input(&in.fmt);
        in.fmt = nullptr;
        in.st = nullptr;
        in.stream_index = -1;
        in.ok = false;
    }

    void clear_ring() {
        ring_.clear();
        ring_bytes_ = 0;
        dts0_ = AV_NOPTS_VALUE;
        wall0_ms_ = 0;
        last_wall_ms_ = 0;
    }

    void push_packet(const AVPacket* pkt, int64_t wall_ms) {
        if (pkt->size <= 0 || pkt->size > 4 * 1024 * 1024) return;
        Pkt p;
        p.data.assign(pkt->data, pkt->data + pkt->size);
        p.dts = pkt->dts;
        p.pts = pkt->pts;
        p.wall_ms = wall_ms;
        p.key = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        ring_bytes_ += static_cast<int64_t>(p.data.size());
        ring_.push_back(std::move(p));
        int keep_ms = (pre_ + margin_) * 1000;
        while (!ring_.empty() &&
               (last_wall_ms_ - ring_.front().wall_ms > keep_ms ||
                ring_bytes_ > max_bytes_)) {
            ring_bytes_ -= static_cast<int64_t>(ring_.front().data.size());
            ring_.pop_front();
        }
    }

    int64_t packet_dts_us(const AVPacket* pkt, const Input& in) const {
        int64_t ts = pkt->dts != AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
        if (ts == AV_NOPTS_VALUE) return AV_NOPTS_VALUE;
        return static_cast<int64_t>(ts * av_q2d(in.tb) * 1000000.0);
    }

    bool write_clip(const Req& req, const Input& in, ClipResult& res) {
        int64_t start_ms = req.alarm_ms - pre_ * 1000LL;
        int64_t end_ms = req.alarm_ms + post_ * 1000LL;
        if (!ring_.empty()) {
            log_info("ring window alarm_ms=" + std::to_string(req.alarm_ms) +
                     " start_ms=" + std::to_string(start_ms) +
                     " oldest_ms=" + std::to_string(ring_.front().wall_ms) +
                     " newest_ms=" + std::to_string(ring_.back().wall_ms) +
                     " pkts=" + std::to_string(ring_.size()) +
                     " bytes=" + std::to_string(ring_bytes_));
        }
        // Prefer the last keyframe at/before start (production also keeps a
        // keyframe-aligned pre-roll; the clip may be a GOP longer than 10s).
        int first = -1;
        for (size_t i = 0; i < ring_.size(); ++i) {
            if (ring_[i].wall_ms > start_ms) break;
            if (ring_[i].key) first = static_cast<int>(i);
        }
        if (first < 0) {
            for (size_t i = 0; i < ring_.size(); ++i) {
                if (ring_[i].wall_ms >= start_ms) {
                    first = static_cast<int>(i);
                    break;
                }
            }
        }
        if (first < 0) {
            res.error = "no packet at start";
            return false;
        }
        int last = first;
        for (size_t i = first; i < ring_.size(); ++i) {
            if (ring_[i].wall_ms <= end_ms) last = static_cast<int>(i);
            else break;
        }
        if (last <= first) {
            res.error = "no packet in window";
            return false;
        }
        if (ring_[first].wall_ms < start_ms - 3000) {
            // Keyframe is very old; prefer a closer packet for duration sanity.
            for (size_t i = 0; i < ring_.size(); ++i) {
                if (ring_[i].wall_ms >= start_ms) {
                    first = static_cast<int>(i);
                    break;
                }
            }
        }
        res.started_keyframe = ring_[first].key ? 1 : 0;

        AVFormatContext* ofmt = nullptr;
        if (avformat_alloc_output_context2(&ofmt, nullptr, "mp4", req.out.c_str()) < 0 ||
            !ofmt) {
            res.error = "alloc output failed";
            return false;
        }
        AVStream* ost = avformat_new_stream(ofmt, nullptr);
        if (!ost || avcodec_parameters_copy(ost->codecpar, in.st->codecpar) < 0) {
            res.error = "copy codecpar failed";
            avformat_free_context(ofmt);
            return false;
        }
        ost->codecpar->codec_tag = 0;
        ost->time_base = in.tb;
        if (avio_open(&ofmt->pb, req.out.c_str(), AVIO_FLAG_WRITE) < 0) {
            res.error = "open output failed";
            avformat_free_context(ofmt);
            return false;
        }
        if (avformat_write_header(ofmt, nullptr) < 0) {
            res.error = "write header failed";
            avio_closep(&ofmt->pb);
            avformat_free_context(ofmt);
            return false;
        }
        int64_t first_dts = AV_NOPTS_VALUE;
        int64_t last_dts = AV_NOPTS_VALUE;
        int written = 0;
        for (int i = first; i <= last; ++i) {
            const Pkt& p = ring_[i];
            AVPacket* op = av_packet_alloc();
            if (!op) break;
            if (av_new_packet(op, static_cast<int>(p.data.size())) < 0) {
                av_packet_free(&op);
                break;
            }
            std::memcpy(op->data, p.data.data(), p.data.size());
            op->pts = p.pts;
            op->dts = p.dts;
            op->flags = p.key ? AV_PKT_FLAG_KEY : 0;
            av_packet_rescale_ts(op, in.tb, ost->time_base);
            if (first_dts == AV_NOPTS_VALUE) first_dts = op->dts;
            op->dts -= first_dts;
            if (op->pts != AV_NOPTS_VALUE) op->pts -= first_dts;
            if (op->dts < 0) op->dts = 0;
            if (last_dts != AV_NOPTS_VALUE && op->dts <= last_dts) op->dts = last_dts + 1;
            if (op->pts != AV_NOPTS_VALUE && op->pts < op->dts) op->pts = op->dts;
            last_dts = op->dts;
            int wr = av_interleaved_write_frame(ofmt, op);
            av_packet_free(&op);
            if (wr < 0) {
                res.error = "write frame failed";
                break;
            }
            ++written;
        }
        AVRational out_tb = ost->time_base;
        av_write_trailer(ofmt);
        avio_closep(&ofmt->pb);
        avformat_free_context(ofmt);
        if (written <= 0) {
            if (res.error.empty()) res.error = "no frame written";
            return false;
        }
        res.ok = true;
        res.frames = written;
        res.duration_ms = last_dts > 0
                              ? static_cast<int64_t>(last_dts * av_q2d(out_tb) * 1000.0)
                              : 0;
        return true;
    }

    void loop() {
        int backoff = 2000;
        while (!stop_) {
            Input in;
            if (!open_input(in)) {
                running_ = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
                backoff = std::min(30000, backoff * 2);
                continue;
            }
            backoff = 2000;
            running_ = true;
            log_info("alarm recorder connected: " + url_);
            AVPacket* pkt = av_packet_alloc();
            while (!stop_) {
                int ret = av_read_frame(in.fmt, pkt);
                if (ret < 0) {
                    log_warn("alarm recorder read: " + std::to_string(ret));
                    break;
                }
                int64_t now = wall_now_ms();
                {
                    std::lock_guard<std::mutex> lk(req_mu_);
                    if (req_.active && !req_.done &&
                        now > req_.hard_deadline_ms) {
                        req_.done = true;
                        req_.ok = false;
                        req_.error = "hard timeout";
                        cv_.notify_all();
                    }
                    if (req_.active && !req_.done && now > req_.deadline_ms) {
                        req_.done = true;
                        req_.ok = false;
                        req_.error = "deadline";
                        cv_.notify_all();
                    }
                }
                if (pkt->stream_index != in.stream_index) {
                    av_packet_unref(pkt);
                    continue;
                }
                int64_t dts_us = packet_dts_us(pkt, in);
                if (dts_us == AV_NOPTS_VALUE) {
                    av_packet_unref(pkt);
                    continue;
                }
                if (dts0_ == AV_NOPTS_VALUE) {
                    dts0_ = dts_us;
                    wall0_ms_ = now;
                }
                int64_t wall_ms = wall0_ms_ + (dts_us - dts0_) / 1000;
                // Live packets should be close to wall clock; clamp absurd
                // backwards jumps so the ring cannot be poisoned.
                if (wall_ms < now - 60000) {
                    wall0_ms_ += (now - wall_ms);
                    wall_ms = now;
                }
                last_wall_ms_ = std::max(last_wall_ms_, wall_ms);
                {
                    std::lock_guard<std::mutex> lk(req_mu_);
                    if (req_.active && !req_.done) {
                        // Post-roll complete? write now.
                        if (wall_ms >= req_.alarm_ms + post_ * 1000LL) {
                            ClipResult res;
                            write_clip(req_, in, res);
                            req_.done = true;
                            req_.ok = res.ok;
                            req_.error = res.error;
                            req_.duration_ms = res.duration_ms;
                            req_.frames = res.frames;
                            req_.started_keyframe = res.started_keyframe;
                            cv_.notify_all();
                        }
                    }
                }
                push_packet(pkt, wall_ms);
                av_packet_unref(pkt);
                // Wake up request writers waiting for a running recorder.
                cv_.notify_all();
            }
            av_packet_free(&pkt);
            close_input(in);
            running_ = false;
            clear_ring();
            if (!stop_) std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        running_ = false;
    }

    std::string url_;
    int pre_ = 5;
    int post_ = 5;
    int margin_ = 5;
    int64_t max_bytes_ = 32 * 1024 * 1024;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    std::thread thread_;

    std::deque<Pkt> ring_;
    int64_t ring_bytes_ = 0;
    int64_t dts0_ = AV_NOPTS_VALUE;
    int64_t wall0_ms_ = 0;
    int64_t last_wall_ms_ = 0;

    std::mutex req_mu_;
    std::condition_variable cv_;
    Req req_;
};

}  // namespace aidetect
