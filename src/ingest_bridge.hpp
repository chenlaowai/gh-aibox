#pragma once

// In-process ingest bridge.
//
// The service used to spawn one `ffmpeg` child per channel to pull the camera
// stream and remux it into MPEG-TS on a local UDP port (the sail decoder then
// reads that port).  Production keeps this stage inside its own process, so we
// do the same here with libavformat: one thread per channel opens the source,
// reads packets, muxes them to MPEG-TS into an avio whose write callback fans
// the bytes out to the decoder port and the alarm-ring port.  No child process,
// no shell, no extra log files.

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace aidetect {

class IngestBridge {
public:
    IngestBridge() = default;
    ~IngestBridge() { stop(); }

    IngestBridge(const IngestBridge&) = delete;
    IngestBridge& operator=(const IngestBridge&) = delete;

    // dec_port: where the sail decoder listens; rec_port: packet ring recorder
    // (0 disables).  use_bsf: force hevc_mp4toannexb (learned by the caller).
    bool start(const std::string& url, int dec_port, int rec_port, bool use_bsf,
               bool loop_file) {
        stop();
        url_ = url;
        dec_port_ = dec_port;
        rec_port_ = rec_port;
        use_bsf_ = use_bsf;
        loop_file_ = loop_file;
        error_.clear();
        running_ = true;
        thread_ = std::thread([this] { loop(); });
        return true;
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    bool running() const { return running_.load(); }

    // Alarm clip: keep the last `pre_s` seconds of encoded packets and write
    // `pre_s + post_s` seconds to an MPEG-TS file.  Production has the same
    // in-process recorder (libdump4); doing it here means no URL re-open and
    // no dependence on wall-clock <-> PTS mapping.
    bool start_clip(const std::string& path, int pre_s, int post_s) {
        std::lock_guard<std::mutex> lk(clip_mu_);
        if (clip_active_) return false;
        clip_pre_s_ = std::max(1, pre_s);
        clip_path_ = path;
        clip_post_s_ = std::max(1, post_s);
        clip_request_ms_ = now_wall_ms();
        clip_done_ = false;
        clip_started_ = false;
        clip_active_ = true;
        return true;
    }
    bool clip_done() const { return clip_done_.load(); }
    bool clip_active() const { return clip_active_.load(); }
    std::string clip_path() { std::lock_guard<std::mutex> lk(clip_mu_); return clip_path_; }

    std::string error() {
        std::lock_guard<std::mutex> lk(mu_);
        return error_;
    }

private:
    static void set_error(std::string& err, const char* what) {
        char buf[256];
        av_strerror(AVERROR(errno), buf, sizeof(buf));
        err = std::string(what ? what : "") + ": " + buf;
    }

    static int write_cb(void* opaque, uint8_t* buf, int size) {
        // The mpegts muxer hands us whatever it buffered (up to 64 KB).  UDP
        // datagrams above the MTU are fragmented and the decoder reads with
        // pkt_size=1316, so it sees truncated TS packets and never produces a
        // frame.  Slice every write into MTU-safe datagrams, like ffmpeg's
        // udp protocol does.
        constexpr int kChunk = 1316;
        auto* self = static_cast<IngestBridge*>(opaque);
        for (int off = 0; off < size; off += kChunk) {
            const int n = std::min(kChunk, size - off);
            if (self->sock_dec_ >= 0)
                ::send(self->sock_dec_, buf + off, n, MSG_NOSIGNAL);
            if (self->sock_rec_ >= 0)
                ::send(self->sock_rec_, buf + off, n, MSG_NOSIGNAL);
        }
        return size;
    }

    static int udp_socket(int port) {
        int s = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) return -1;
        int sndbuf = 2 * 1024 * 1024;
        ::setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(s);
            return -1;
        }
        return s;
    }

    void fail(const std::string& msg) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            error_ = msg;
        }
        running_ = false;
    }

    void loop() {
        AVFormatContext* in = nullptr;
        AVFormatContext* out = nullptr;
        AVBSFContext* bsf = nullptr;
        AVIOContext* avio = nullptr;
        uint8_t* aviobuf = nullptr;

        sock_dec_ = udp_socket(dec_port_);
        sock_rec_ = rec_port_ > 0 ? udp_socket(rec_port_) : -1;
        if (sock_dec_ < 0) {
            fail("ingest: cannot open decoder udp port");
            return;
        }

        AVDictionary* opts = nullptr;
        if (url_.rfind("rtsp://", 0) == 0) {
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            av_dict_set(&opts, "stimeout", "20000000", 0);
        }

        int ret = avformat_open_input(&in, url_.c_str(), nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            fail("ingest open: " + std::string(av_err2str(ret)));
            // A failed avformat_open_input can still leave a partially
            // allocated context; every failed ISC retry would leak it and the
            // process RSS grew ~2 MB/min until this was closed.
            if (in) avformat_close_input(&in);
            cleanup(nullptr, nullptr, nullptr, nullptr, sock_dec_, sock_rec_);
            return;
        }
        if (avformat_find_stream_info(in, nullptr) < 0) {
            fail("ingest: no stream info");
            goto done;
        }

        {
            const int vstream = av_find_best_stream(in, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            if (vstream < 0) {
                fail("ingest: no video stream");
                goto done;
            }
            AVStream* vin = in->streams[vstream];

            if (avformat_alloc_output_context2(&out, nullptr, "mpegts", nullptr) < 0 || !out) {
                fail("ingest: mpegts muxer unavailable");
                goto done;
            }
            AVStream* vout = avformat_new_stream(out, nullptr);
            if (!vout || avcodec_parameters_copy(vout->codecpar, vin->codecpar) < 0) {
                fail("ingest: cannot copy codec parameters");
                goto done;
            }
            vout->codecpar->codec_tag = 0;
            vout->time_base = vin->time_base;

            if (use_bsf_) {
                const AVBitStreamFilter* f = av_bsf_get_by_name("hevc_mp4toannexb");
                if (f) {
                    if (av_bsf_alloc(f, &bsf) == 0 &&
                        avcodec_parameters_copy(bsf->par_in, vin->codecpar) == 0) {
                        bsf->time_base_in = vin->time_base;
                        if (av_bsf_init(bsf) < 0) {
                            av_bsf_free(&bsf);
                            bsf = nullptr;
                        }
                    } else {
                        av_bsf_free(&bsf);
                        bsf = nullptr;
                    }
                }
            }

            aviobuf = static_cast<uint8_t*>(av_malloc(65536));
            if (!aviobuf) {
                fail("ingest: out of memory");
                goto done;
            }
            avio = avio_alloc_context(aviobuf, 65536, 1, this, nullptr, &IngestBridge::write_cb,
                                      nullptr);
            if (!avio) {
                fail("ingest: cannot allocate avio");
                goto done;
            }
            out->pb = avio;
            out->flags |= AVFMT_FLAG_CUSTOM_IO;
            if (avformat_write_header(out, nullptr) < 0) {
                fail("ingest: cannot write mpegts header");
                goto done;
            }

            AVPacket* pkt = av_packet_alloc();
            if (!pkt) {
                fail("ingest: cannot allocate packet");
                goto done;
            }
            int64_t last_pkt_us = av_gettime_relative();
            int64_t ts_off = 0;
            int64_t last_pts = AV_NOPTS_VALUE;
            while (running_) {
                ret = av_read_frame(in, pkt);
                if (ret < 0) {
                    if (loop_file_ && ret == AVERROR_EOF) {
                        // Seek back for a seamless loop.  Reopening the input left a
                        // 100~300ms hole in the TS every loop; the decoder then reported a
                        // read timeout and the tile flashed "正在重连...".  The monotonic
                        // timestamp offset below turns the pts restart into a forward jump.
                        if (av_seek_frame(in, -1, 0, AVSEEK_FLAG_BACKWARD) < 0) {
                            fail("ingest loop seek failed");
                            break;
                        }
                        avformat_flush(in);
                        continue;
                    }
                    fail("ingest read: " + std::string(av_err2str(ret)));
                    break;
                }
                if (pkt->stream_index != vstream) {
                    av_packet_unref(pkt);
                    continue;
                }
                if (bsf) {
                    if (av_bsf_send_packet(bsf, pkt) < 0) {
                        av_packet_unref(pkt);
                        continue;
                    }
                    if (av_bsf_receive_packet(bsf, pkt) < 0) {
                        av_packet_unref(pkt);
                        continue;
                    }
                }
                av_packet_rescale_ts(pkt, vin->time_base, vout->time_base);
                // A looped MP4 restarts at pts 0.  Keep the output timeline
                // monotonic instead of jumping backwards, otherwise the
                // decoder treats every loop as a discontinuity and the task
                // reconnects (that reconnect storm was the RSS leak driver).
                if (pkt->pts != AV_NOPTS_VALUE) {
                    if (last_pts != AV_NOPTS_VALUE &&
                        pkt->pts + ts_off < last_pts) {
                        ts_off = last_pts - pkt->pts + 3600;
                    }
                    pkt->pts += ts_off;
                    if (pkt->dts != AV_NOPTS_VALUE) pkt->dts += ts_off;
                    last_pts = pkt->pts;
                }
                pkt->stream_index = 0;
                pkt->pos = -1;
                if (av_interleaved_write_frame(out, pkt) < 0) {
                    // A short write only means the UDP receiver dropped a
                    // datagram; keep going, the decoder tolerates gaps.
                }
                {
                    std::lock_guard<std::mutex> lk(clip_mu_);
                    if (clip_active_.load()) {
                        if (!clip_started_) clip_open(vin);
                        if (clip_out_) clip_write(pkt);
                        if (clip_started_ &&
                            now_wall_ms() > clip_request_ms_ + clip_post_s_ * 1000LL)
                            clip_close();
                    }
                    ring_push(pkt);
                }
                av_packet_unref(pkt);
                // MP4 playback must not outrun real time (ffmpeg's -re).
                if (loop_file_) {
                    last_pkt_us = av_gettime_relative();
                }
            }
            av_packet_free(&pkt);
            av_write_trailer(out);
        }

    done:
        if (bsf) av_bsf_free(&bsf);
        if (out) avformat_free_context(out);
        avio_context_free(&avio);
        if (aviobuf) av_free(aviobuf);
        if (in) avformat_close_input(&in);
        cleanup(nullptr, nullptr, nullptr, nullptr, sock_dec_, sock_rec_);
        sock_dec_ = sock_rec_ = -1;
        running_ = false;
    }

    static void cleanup(void*, void*, void*, void*, int dec, int rec) {
        if (dec >= 0) ::close(dec);
        if (rec >= 0) ::close(rec);
    }

    struct RingPkt {
        AVPacket* pkt = nullptr;
        int64_t arrival_ms = 0;
    };
    std::deque<RingPkt> ring_;
    std::mutex clip_mu_;
    AVFormatContext* clip_out_ = nullptr;
    AVStream* clip_stream_ = nullptr;
    bool clip_started_ = false;
    int clip_pre_s_ = 5;
    int clip_post_s_ = 5;
    int64_t clip_request_ms_ = 0;
    std::string clip_path_;
    std::atomic<bool> clip_active_{false};
    std::atomic<bool> clip_done_{false};
    static int64_t now_wall_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
    // Called from the ingest thread for every encoded video packet.
    void ring_push(const AVPacket* pkt) {
        auto* dup = av_packet_clone(pkt);
        if (!dup) return;
        ring_.push_back({dup, now_wall_ms()});
        const int64_t keep_ms = (clip_pre_s_ + 4) * 1000LL;
        const int64_t now = now_wall_ms();
        while (!ring_.empty() && now - ring_.front().arrival_ms > keep_ms) {
            av_packet_free(&ring_.front().pkt);
            ring_.pop_front();
        }
    }
    void clip_write(const AVPacket* pkt) {
        if (!clip_out_ || !clip_stream_) return;
        AVPacket* copy = av_packet_clone(pkt);
        if (!copy) return;
        copy->stream_index = 0;
        copy->pos = -1;
        av_interleaved_write_frame(clip_out_, copy);
        av_packet_free(&copy);
    }
    // Opens the clip muxer and flushes the pre-alarm window from the ring.
    void clip_open(AVStream* vin) {
        if (avformat_alloc_output_context2(&clip_out_, nullptr, "mpegts",
                                           clip_path_.c_str()) < 0 || !clip_out_) {
            clip_out_ = nullptr;
            clip_active_ = false;
            return;
        }
        clip_stream_ = avformat_new_stream(clip_out_, nullptr);
        if (!clip_stream_ ||
            avcodec_parameters_copy(clip_stream_->codecpar, vin->codecpar) < 0) {
            avformat_free_context(clip_out_);
            clip_out_ = nullptr;
            clip_active_ = false;
            return;
        }
        clip_stream_->codecpar->codec_tag = 0;
        clip_stream_->time_base = vin->time_base;
        if (avio_open(&clip_out_->pb, clip_path_.c_str(), AVIO_FLAG_WRITE) < 0) {
            avformat_free_context(clip_out_);
            clip_out_ = nullptr;
            clip_active_ = false;
            return;
        }
        if (avformat_write_header(clip_out_, nullptr) < 0) {
            avio_closep(&clip_out_->pb);
            avformat_free_context(clip_out_);
            clip_out_ = nullptr;
            clip_active_ = false;
            return;
        }
        clip_started_ = true;
        const int64_t from_ms = clip_request_ms_ - clip_pre_s_ * 1000LL;
        for (auto& rp : ring_)
            if (rp.arrival_ms >= from_ms) clip_write(rp.pkt);
    }
    void clip_close() {
        if (!clip_out_) return;
        av_write_trailer(clip_out_);
        if (clip_out_->pb) avio_closep(&clip_out_->pb);
        avformat_free_context(clip_out_);
        clip_out_ = nullptr;
        clip_stream_ = nullptr;
        clip_active_ = false;
        clip_done_ = true;
    }
    std::string url_;
    int dec_port_ = 0;
    int rec_port_ = 0;
    bool use_bsf_ = false;
    bool loop_file_ = false;
    int sock_dec_ = -1;
    int sock_rec_ = -1;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mu_;
    std::string error_;
};

}  // namespace aidetect
