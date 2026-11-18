// Route-B probe: decode a stream with the Sophon VPU and report what an
// AVFrame actually carries (pixel format, plane pointers, strides).
// Not shipped; used to pin down the direct bm decoder contract.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <dirent.h>
#include <sys/resource.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

static int count_fds() {
    DIR* d = opendir("/proc/self/fd");
    if (!d) return -1;
    int n = 0;
    while (readdir(d)) n++;
    closedir(d);
    return n;
}

static int count_dmabuf() {
    // /proc/self/maps lines containing /dmabuf
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, "dmabuf")) n++;
    fclose(f);
    return n;
}

static double cpu_ms() {
    rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000.0 +
           (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000.0;
}
static double now_ms() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: %s <url> [decoder] [frames]\n", argv[0]);
        return 2;
    }
    const char* url = argv[1];
    const char* dec_name = argc > 2 ? argv[2] : "h264_bm";
    int want = argc > 3 ? atoi(argv[3]) : 60;

    av_log_set_level(AV_LOG_ERROR);
    avformat_network_init();

    AVFormatContext* fmt = nullptr;
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "stimeout", "8000000", 0);
    av_dict_set(&opts, "buffer_size", "4194304", 0);
    int ret = avformat_open_input(&fmt, url, nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) { printf("open failed %d\n", ret); return 3; }
    if (avformat_find_stream_info(fmt, nullptr) < 0) { printf("no stream info\n"); return 3; }
    int vs = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (vs < 0) { printf("no video stream\n"); return 3; }
    AVStream* st = fmt->streams[vs];
    printf("stream codec=%s %dx%d\n", avcodec_get_name(st->codecpar->codec_id),
           st->codecpar->width, st->codecpar->height);

    const AVCodec* dec = avcodec_find_decoder_by_name(dec_name);
    if (!dec) { printf("decoder %s not found\n", dec_name); return 3; }
    AVCodecContext* cc = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(cc, st->codecpar);
    cc->thread_count = 1;
    if ((ret = avcodec_open2(cc, dec, nullptr)) < 0) { printf("open2 failed %d\n", ret); return 3; }
    printf("decoder=%s w=%d h=%d pix_fmt=%d\n", dec->name, cc->width, cc->height, cc->pix_fmt);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int got = 0, errors = 0;
    double t0 = now_ms();
    double c0 = cpu_ms();
    double first_t = 0;
    int fd0 = count_fds(), dm0 = count_dmabuf();

    while (got < want && av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index != vs) { av_packet_unref(pkt); continue; }
        if ((ret = avcodec_send_packet(cc, pkt)) < 0) {
            errors++;
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);
        while (avcodec_receive_frame(cc, frame) == 0) {
            if (got == 0) {
                first_t = now_ms();
                printf("first frame after %.1f ms\n", first_t - t0);
                printf("  format=%d (%s) w=%d h=%d coded=%dx%d\n", frame->format,
                       av_get_pix_fmt_name((AVPixelFormat)frame->format), frame->width,
                       frame->height, cc->coded_width, cc->coded_height);
                for (int i = 0; i < 8; i++)
                    printf("  data[%d]=%p linesize[%d]=%d\n", i, frame->data[i], i,
                           frame->linesize[i]);
                printf("  buf[0]=%p buf[0]->size=%d\n",
                       (void*)(frame->buf[0] ? frame->buf[0]->data : nullptr),
                       frame->buf[0] ? frame->buf[0]->size : -1);
            }
            got++;
            av_frame_unref(frame);
            if (got >= want) break;
        }
    }
    double t1 = now_ms();
    printf("frames=%d errors=%d elapsed=%.1fms fps=%.1f\n", got, errors, t1 - t0,
           got * 1000.0 / (t1 - t0));
    printf("cpu=%.0fms cpu_per_frame=%.2fms\n", cpu_ms() - c0, (cpu_ms() - c0) / got);
    printf("fd %d -> %d, dmabuf maps %d -> %d\\n", fd0, count_fds(), dm0, count_dmabuf());

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&cc);
    avformat_close_input(&fmt);
    return 0;
}
