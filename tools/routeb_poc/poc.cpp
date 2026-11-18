// Route-B PoC: sophon-ffmpeg VPU decode -> bmcv -> bmrt -> draw -> JPEG.
// No libsail. Purpose: prove the production stack works directly and measure
// CPU/RSS/fd/dmabuf before porting the service.
//
// build (on the box):
//   source env/env.sh
//   g++ -std=c++17 -O2 tools/routeb_poc/poc.cpp -o /tmp/routeb_poc \
//     -I$FFMPEG_HOME/include -I$LIBSOPHON_HOME/include \
//     -L$FFMPEG_HOME/lib -L$LIBSOPHON_HOME/lib \
//     -Wl,-rpath,$FFMPEG_HOME/lib -Wl,-rpath,$LIBSOPHON_HOME/lib \
//     -lavformat -lavcodec -lavutil -lbmcv -lbmrt -lbmlib
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <dirent.h>
#include <sys/resource.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <bmlib_runtime.h>
#include <bmcv_api_ext.h>
#include <bmruntime_interface.h>
#include <bmdef.h>
}

static double now_ms() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
static long rss_kb() {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long v = -1;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "VmRSS: %ld kB", &v) == 1) break;
    fclose(f);
    return v;
}
static int count_fds() {
    DIR* d = opendir("/proc/self/fd");
    if (!d) return -1;
    int n = 0;
    while (readdir(d)) n++;
    closedir(d);
    return n;
}
static int count_maps(const char* needle) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, needle)) n++;
    fclose(f);
    return n;
}
static double cpu_ms() {
    rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000.0 +
           (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000.0;
}
static void shape_str(const bm_shape_t& s, char* out, size_t n) {
    out[0] = 0;
    for (int i = 0; i < s.num_dims; i++) {
        char t[24];
        snprintf(t, sizeof(t), "%s%d", i ? "," : "", s.dims[i]);
        strncat(out, t, n - strlen(out) - 1);
    }
}

struct Det { float x1, y1, x2, y2, score; int cls; };

// Generic YOLO head: handles [1,C,N] (v8/v11, no objectness) and
// [1,N,C] with/without objectness (v5). Only used to prove the tensor path.
static std::vector<Det> parse_yolo(const std::vector<float>& d, const bm_shape_t& sh,
                                   float conf, int img_w, int img_h, int in_w,
                                   int in_h) {
    std::vector<Det> out;
    if (sh.num_dims != 3) return out;
    int d1 = sh.dims[1], d2 = sh.dims[2];
    bool channel_first = d1 <= d2;  // [1,C,N]
    int C = channel_first ? d1 : d2;
    int N = channel_first ? d2 : d1;
    if (C < 6 || C > 4096) return out;
    auto at = [&](int c, int i) {
        return channel_first ? d[(size_t)c * N + i] : d[(size_t)i * C + c];
    };
    bool obj = (C == 85 || C == 6 || C == 7);
    for (int i = 0; i < N; i++) {
        float best = 0;
        int bc = -1;
        for (int c = 4; c < C; c++) {
            float s = at(c, i);
            if (s > best) { best = s; bc = c - 4; }
        }
        float score = obj ? at(4, i) * best : best;
        if (score < conf) continue;
        float cx = at(0, i), cy = at(1, i), w = at(2, i), h = at(3, i);
        // letterbox scale (aspect preserved, centered)
        float sx = (float)in_w / img_w, sy = (float)in_h / img_h;
        float s = std::min(sx, sy);
        float px = (in_w - img_w * s) / 2, py = (in_h - img_h * s) / 2;
        Det det;
        det.x1 = (cx - w / 2 - px) / s;
        det.y1 = (cy - h / 2 - py) / s;
        det.x2 = (cx + w / 2 - px) / s;
        det.y2 = (cy + h / 2 - py) / s;
        det.score = score;
        det.cls = bc;
        out.push_back(det);
    }
    std::sort(out.begin(), out.end(), [](const Det& a, const Det& b) {
        return a.score > b.score;
    });
    if (out.size() > 100) out.resize(100);
    return out;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: %s <url> <model.bmodel> [frames] [out.jpg] [conf]\n", argv[0]);
        return 2;
    }
    const char* url = argv[1];
    const char* model = argv[2];
    int want = argc > 3 ? atoi(argv[3]) : 120;
    const char* out_jpg = argc > 4 ? argv[4] : "/tmp/routeb_poc.jpg";
    float conf = argc > 5 ? (float)atof(argv[5]) : 0.25f;

    av_log_set_level(AV_LOG_ERROR);
    avformat_network_init();

    double wall0 = now_ms();
    long rss0 = rss_kb();
    int fd0 = count_fds(), dm0 = count_maps("dmabuf");
    double cpu0 = cpu_ms();

    bm_handle_t handle = nullptr;
    if (bm_dev_request(&handle, 0) != BM_SUCCESS) { printf("bm_dev_request failed\n"); return 3; }

    void* rt = bmrt_create(handle);
    if (!rt) { printf("bmrt_create failed\n"); return 3; }
    if (!bmrt_load_bmodel(rt, model)) { printf("load bmodel failed: %s\n", model); return 3; }
    int net_num = bmrt_get_network_number(rt);
    const char* net = bmrt_get_network_name(rt, 0);
    const bm_net_info_t* ni = bmrt_get_network_info(rt, net);
    if (!ni) { printf("no net info\n"); return 3; }
    char sh[128];
    shape_str(ni->stages[0].input_shapes[0], sh, sizeof(sh));
    printf("model=%s nets=%d net=%s input=%s dtype=%d outputs=%d\n", model, net_num, net,
           sh, (int)ni->input_dtypes[0], ni->output_num);
    int in_w = ni->stages[0].input_shapes[0].dims[3];
    int in_h = ni->stages[0].input_shapes[0].dims[2];
    bool fp32_in = ni->input_dtypes[0] == BM_FLOAT32;
    for (int i = 0; i < ni->output_num; i++) {
        shape_str(ni->stages[0].output_shapes[i], sh, sizeof(sh));
        printf("  out[%d] %s shape=%s dtype=%d\n", i, ni->output_names[i], sh,
               (int)ni->output_dtypes[i]);
    }

    // Output tensors are allocated once and reused every frame.
    std::vector<bm_tensor_t> outs(ni->output_num);
    for (int i = 0; i < ni->output_num; i++)
        bmrt_tensor_ex(&outs[i], rt, 0, ni->output_dtypes[i], ni->stages[0].output_shapes[i]);

    // Model input image (letterbox target) and optional fp32 copy.
    bm_image in_img, in_f32;
    bm_image_create(handle, in_h, in_w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &in_img);
    bm_image_alloc_dev_mem(in_img, 1);
    if (fp32_in) {
        bm_image_create(handle, in_h, in_w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_FLOAT32, &in_f32);
        bm_image_alloc_dev_mem(in_f32, 1);
    }

    // Decode: VPU ("h264_bm"/"hevc_bm"), frames expose device addresses in
    // data[4]/data[5] and strides in linesize[4]/linesize[5].
    AVFormatContext* fmt = nullptr;
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "stimeout", "8000000", 0);
    av_dict_set(&opts, "buffer_size", "4194304", 0);
    if (avformat_open_input(&fmt, url, nullptr, &opts) < 0) { printf("open failed\n"); return 3; }
    av_dict_free(&opts);
    if (avformat_find_stream_info(fmt, nullptr) < 0) { printf("no stream info\n"); return 3; }
    int vs = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream* st = fmt->streams[vs];
    const char* dec_name = st->codecpar->codec_id == AV_CODEC_ID_HEVC ? "hevc_bm" : "h264_bm";
    const AVCodec* dec = avcodec_find_decoder_by_name(dec_name);
    AVCodecContext* cc = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(cc, st->codecpar);
    cc->thread_count = 1;
    if (avcodec_open2(cc, dec, nullptr) < 0) { printf("decoder open failed\n"); return 3; }
    printf("stream %dx%d codec=%s decoder=%s\n", st->codecpar->width, st->codecpar->height,
           avcodec_get_name(st->codecpar->codec_id), dec_name);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bm_image nv12;
    bool nv12_ready = false;
    bm_image preview, preview_nv12;
    bm_image_create(handle, 720, 1280, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE, &preview);
    bm_image_alloc_dev_mem(preview, 1);
    bm_image_create(handle, 720, 1280, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &preview_nv12);
    bm_image_alloc_dev_mem(preview_nv12, 1);

    int frames = 0, det_frames = 0, fails = 0;
    double infer_ms = 0, decode_ms = 0, conv_ms = 0;
    std::vector<Det> last;
    double first_frame_ms = -1;

    while (frames < want && av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index != vs) { av_packet_unref(pkt); continue; }
        double t_pt = now_ms();
        int sret = avcodec_send_packet(cc, pkt);
        av_packet_unref(pkt);
        if (sret < 0) { fails++; continue; }
        while (avcodec_receive_frame(cc, frame) == 0) {
            if (first_frame_ms < 0) first_frame_ms = now_ms() - wall0;
            int w = frame->width, h = frame->height;
            // attach the decoded device planes as an NV12 image (no copy)
            if (!nv12_ready) {
                int stride[2] = {frame->linesize[4], frame->linesize[5]};
                if (bm_image_create(handle, h, w, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &nv12,
                                    stride) != BM_SUCCESS) {
                    printf("nv12 create failed\n");
                    return 4;
                }
                nv12_ready = true;
            }
            bm_device_mem_t planes[2];
            planes[0] = bm_mem_from_device((unsigned long long)frame->data[4], h * frame->linesize[4]);
            planes[1] = bm_mem_from_device((unsigned long long)frame->data[5],
                                           h / 2 * frame->linesize[5]);
            if (bm_image_attach(nv12, planes) != BM_SUCCESS) { printf("attach failed\n"); return 4; }

            // letterbox to the model input size (whole frame -> in_w x in_h)
            bmcv_rect_t crop = {0, 0, w, h};
            double t0 = now_ms();
            if (bmcv_image_vpp_convert(handle, 1, nv12, &in_img, &crop) != BM_SUCCESS) {
                fails++;
                av_frame_unref(frame);
                continue;
            }
            if (fp32_in) {
                bmcv_convert_to_attr attr{};
                attr.alpha_0 = attr.alpha_1 = attr.alpha_2 = 1.0f / 255.0f;
                attr.beta_0 = attr.beta_1 = attr.beta_2 = 0.0f;
                bmcv_image_convert_to(handle, 1, attr, &in_img, &in_f32);
            }

            bm_tensor_t in_t;
            bm_device_mem_t in_mem;
            bm_image_get_device_mem(fp32_in ? in_f32 : in_img, &in_mem);
            bmrt_tensor_with_device(&in_t, in_mem, fp32_in ? BM_FLOAT32 : BM_UINT8,
                                    ni->stages[0].input_shapes[0]);
            double ti = now_ms();
            if (!bmrt_launch_tensor_ex(rt, net, &in_t, 1, outs.data(), ni->output_num, true,
                                       false)) {
                printf("launch failed\n");
                return 4;
            }
            infer_ms += now_ms() - ti;
            conv_ms += ti - t0;

            // copy first output back and run the generic YOLO decode
            std::vector<float> host(bmrt_shape_count(&outs[0].shape));
            bm_memcpy_d2s(handle, host.data(), outs[0].device_mem);
            last = parse_yolo(host, outs[0].shape, conf, w, h, in_w, in_h);
            if (!last.empty()) det_frames++;

            // draw + JPEG on a 1280x720 preview (device side, then encode)
            bmcv_image_vpp_convert(handle, 1, nv12, &preview, &crop);
            if (!last.empty()) {
                std::vector<bmcv_rect_t> rects;
                for (auto& d : last) {
                    float sx = 1280.0f / w, sy = 720.0f / h;
                    int x = std::max(0, (int)(d.x1 * sx)), y = std::max(0, (int)(d.y1 * sy));
                    int rw = std::min(1280 - x, (int)((d.x2 - d.x1) * sx));
                    int rh = std::min(720 - y, (int)((d.y2 - d.y1) * sy));
                    if (rw > 1 && rh > 1) rects.push_back({x, y, rw, rh});
                    if (rects.size() >= 20) break;
                }
                if (!rects.empty())
                    bmcv_image_draw_rectangle(handle, preview, (int)rects.size(), rects.data(), 2,
                                              0, 0, 255);
            }
            void* jpg = nullptr;
            size_t jsz = 0;
            if (frames == want - 1) {
                // JPEG encoder takes NV12 only: convert the drawn preview.
                bmcv_image_vpp_convert(handle, 1, preview, &preview_nv12);
                if (bmcv_image_jpeg_enc(handle, 1, &preview_nv12, &jpg, &jsz, 90, 0) ==
                    BM_SUCCESS) {
                    FILE* f = fopen(out_jpg, "wb");
                    if (f) { fwrite(jpg, 1, jsz, f); fclose(f); }
                    printf("jpeg %s %zu bytes dets=%zu\n", out_jpg, jsz, last.size());
                    free(jpg);
                } else {
                    printf("jpeg encode failed\n");
                }
            }
            decode_ms += now_ms() - t_pt - (now_ms() - ti);
            frames++;
            av_frame_unref(frame);
            if (frames >= want) break;
        }
    }

    double total = now_ms() - wall0;
    double cpu = cpu_ms() - cpu0;
    printf("frames=%d det_frames=%d fails=%d first_frame=%.0fms\n", frames, det_frames, fails,
           first_frame_ms);
    printf("wall=%.0fms fps=%.2f cpu=%.0fms cpu_per_frame=%.2fms infer_per_frame=%.2fms conv_per_frame=%.2fms\n", total,
           frames * 1000.0 / total, cpu, cpu / std::max(1, frames), infer_ms / std::max(1, frames), conv_ms / std::max(1, frames));
    printf("rss %ld -> %ld kB | fd %d -> %d | dmabuf maps %d -> %d\n", rss0, rss_kb(), fd0,
           count_fds(), dm0, count_maps("dmabuf"));

    if (nv12_ready) bm_image_destroy(nv12);
    bm_image_destroy(preview);
    bm_image_destroy(preview_nv12);
    bm_image_destroy(in_img);
    if (fp32_in) bm_image_destroy(in_f32);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&cc);
    avformat_close_input(&fmt);
    bmrt_destroy(rt);
    bm_dev_free(handle);
    return 0;
}
