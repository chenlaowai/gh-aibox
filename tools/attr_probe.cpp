// Attribute-crop probe: detect persons with people_big, then run the 4-head
// helmet/vest classifier on several crop variants of each person box.
// usage: attr_probe people.bmodel helmet.bmodel image.jpg [people_conf]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <sail/cvwrapper.h>
#include <sail/engine.h>
#include <sail/tensor.h>

struct Box { float x1, y1, x2, y2, s; };

static float iou(const Box& a, const Box& b) {
    float x1 = std::max(a.x1, b.x1), y1 = std::max(a.y1, b.y1);
    float x2 = std::min(a.x2, b.x2), y2 = std::min(a.y2, b.y2);
    float w = std::max(0.f, x2 - x1), h = std::max(0.f, y2 - y1);
    float inter = w * h;
    float ua = (a.x2 - a.x1) * (a.y2 - a.y1) + (b.x2 - b.x1) * (b.y2 - b.y1) - inter;
    return ua > 0 ? inter / ua : 0.f;
}

static void nms(std::vector<Box>& v, float th) {
    std::sort(v.begin(), v.end(), [](const Box& a, const Box& b) { return a.s > b.s; });
    std::vector<Box> out;
    for (auto& b : v) {
        bool keep = true;
        for (auto& o : out) if (iou(b, o) > th) { keep = false; break; }
        if (keep) out.push_back(b);
    }
    v.swap(out);
}

// Run one detector on the whole image, return boxes in image coordinates.
static std::vector<Box> detect(sail::Engine& engine, sail::Handle& handle,
                               sail::Bmcv& bmcv, cv::Mat& img, float conf,
                               float alpha, float beta) {
    std::string graph = engine.get_graph_names()[0];
    auto in_name = engine.get_input_names(graph)[0];
    auto in_shape = engine.get_input_shape(graph, in_name);
    int nw = in_shape[3], nh = in_shape[2];
    int w = img.cols, h = img.rows;
    float r = std::min((float)nw / w, (float)nh / h);
    int tw = (int)std::round(w * r), th = (int)std::round(h * r);
    int tx = (nw - tw) / 2, ty = (nh - th) / 2;
    sail::PaddingAtrr pad;
    pad.set_stx(tx); pad.set_sty(ty); pad.set_w(tw); pad.set_h(th);
    pad.set_r(114); pad.set_g(114); pad.set_b(114);
    sail::BMImage src = sail::Bmcv::mat_to_bm_image(img);
    sail::BMImage resized = bmcv.crop_and_resize_padding(src, 0, 0, w, h, nw, nh, pad,
                                                         bmcv_resize_algorithm::BMCV_INTER_LINEAR);
    const bool use_bgr = std::getenv("ATTR_BGR") != nullptr;
            sail::BMImage pre(handle, nh, nw, use_bgr ? FORMAT_BGR_PLANAR : FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32);
    bmcv.convert_to(resized, pre, std::make_tuple(std::make_pair(alpha, beta),
                                                  std::make_pair(alpha, beta),
                                                  std::make_pair(alpha, beta)));
    sail::Tensor input(handle, in_shape, engine.get_input_dtype(graph, in_name), true, true);
    bmcv.bm_image_to_tensor(pre, input);
    std::map<std::string, sail::Tensor*> in_map{{in_name, &input}};
    std::map<std::string, std::vector<int>> in_shapes{{in_name, in_shape}};
    auto out_names = engine.get_output_names(graph);
    std::vector<std::unique_ptr<sail::Tensor>> storage;
    std::map<std::string, sail::Tensor*> out_map;
    for (auto& n : out_names) {
        auto s = engine.get_output_shape(graph, n);
        auto t = std::make_unique<sail::Tensor>(handle, s, engine.get_output_dtype(graph, n), true, true);
        out_map[n] = t.get();
        storage.push_back(std::move(t));
    }
    engine.process(graph, in_map, in_shapes, out_map);
    auto& t = *storage[0];
    t.sync_d2s();
    const float* p = static_cast<const float*>(t.sys_data());
    auto sh = engine.get_output_shape(graph, out_names[0]);
    int n = sh[1], cols = sh[2];
    std::vector<Box> boxes;
    for (int i = 0; i < n; ++i) {
        const float* q = p + (size_t)i * cols;
        if (q[4] < conf) continue;
        Box b{q[0], q[1], q[2], q[3], q[4]};
        b.x1 = (b.x1 - tx) / r; b.y1 = (b.y1 - ty) / r;
        b.x2 = (b.x2 - tx) / r; b.y2 = (b.y2 - ty) / r;
        b.x1 = std::max(0.f, b.x1); b.y1 = std::max(0.f, b.y1);
        b.x2 = std::min((float)w, b.x2); b.y2 = std::min((float)h, b.y2);
        if (b.x2 > b.x1 && b.y2 > b.y1) boxes.push_back(b);
    }
    nms(boxes, 0.5f);
    return boxes;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("usage: %s people.bmodel helmet.bmodel image.jpg [conf]\n", argv[0]);
        return 2;
    }
    float conf = argc > 4 ? atof(argv[4]) : 0.3f;
    cv::Mat img = cv::imread(argv[3], cv::IMREAD_COLOR);
    if (img.empty()) { printf("image read failed\n"); return 3; }
    sail::Handle handle(0);
    sail::Bmcv bmcv(handle);
    sail::Engine people(argv[1], 0, sail::IOMode::SYSIO);
    sail::Engine helmet(argv[2], 0, sail::IOMode::SYSIO);
    std::vector<Box> persons;
    if (argc >= 9) {
        float x = (float)atof(argv[5]), y = (float)atof(argv[6]);
        float bw = (float)atof(argv[7]), bh = (float)atof(argv[8]);
        persons.push_back(Box{x, y, x + bw, y + bh, 1.f});
        printf("frame=%dx%d rect=%.0f,%.0f %.0fx%.0f\n", img.cols, img.rows, x, y, bw, bh);
    } else {
        persons = detect(people, handle, bmcv, img, conf, 1.0f / 255.0f, 0.0f);
        printf("frame=%dx%d persons=%zu\n", img.cols, img.rows, persons.size());
    }

    std::string graph = helmet.get_graph_names()[0];
    auto in_name = helmet.get_input_names(graph)[0];
    auto in_shape = helmet.get_input_shape(graph, in_name);
    int nw = in_shape[3], nh = in_shape[2];
    auto out_names = helmet.get_output_names(graph);
    cv::Mat resized_buf(nh, nw, CV_8UC3);

    for (size_t i = 0; i < persons.size(); ++i) {
        auto& b = persons[i];
        float bw = b.x2 - b.x1, bh = b.y2 - b.y1;
        printf("--- person[%zu] box=%.0f,%.0f %.0fx%.0f score=%.3f\n", i, b.x1, b.y1, bw, bh, b.s);
        for (float ratio : {0.30f, 0.50f, 0.70f, 1.00f}) {
            int cx = (int)b.x1, cy = (int)b.y1;
            int cw = (int)bw, ch = (int)(bh * ratio);
            cx = std::max(0, std::min(cx, img.cols - 2));
            cy = std::max(0, std::min(cy, img.rows - 2));
            cw = std::min(cw, img.cols - cx); ch = std::min(ch, img.rows - cy);
            if (cw < 8 || ch < 8) continue;
            cv::Mat crop = img(cv::Rect(cx, cy, cw, ch));
            float r = std::min((float)nw / cw, (float)nh / ch);
            int tw = (int)std::round(cw * r), th = (int)std::round(ch * r);
            sail::PaddingAtrr pad;
            pad.set_stx((nw - tw) / 2); pad.set_sty((nh - th) / 2);
            pad.set_w(tw); pad.set_h(th);
            pad.set_r(114); pad.set_g(114); pad.set_b(114);
            sail::BMImage src = sail::Bmcv::mat_to_bm_image(crop);
            sail::BMImage rs = std::getenv("ATTR_STRETCH")
                ? bmcv.crop_and_resize(src, 0, 0, cw, ch, nw, nh,
                                       bmcv_resize_algorithm::BMCV_INTER_LINEAR)
                : bmcv.crop_and_resize_padding(src, 0, 0, cw, ch, nw, nh, pad,
                                               bmcv_resize_algorithm::BMCV_INTER_LINEAR);
            const bool use_bgr = std::getenv("ATTR_BGR") != nullptr;
            sail::BMImage pre(handle, nh, nw, use_bgr ? FORMAT_BGR_PLANAR : FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32);
            const float a_alpha = argc > 10 ? (float)atof(argv[10]) : 1.0f;
            const float a_beta = argc > 11 ? (float)atof(argv[11]) : 0.0f;
            bmcv.convert_to(rs, pre, std::make_tuple(std::make_pair(a_alpha, a_beta),
                                                     std::make_pair(a_alpha, a_beta),
                                                     std::make_pair(a_alpha, a_beta)));
            sail::Tensor input(handle, in_shape, helmet.get_input_dtype(graph, in_name), true, true);
            bmcv.bm_image_to_tensor(pre, input);
            std::map<std::string, sail::Tensor*> in_map{{in_name, &input}};
            std::map<std::string, std::vector<int>> in_shapes{{in_name, in_shape}};
            std::vector<std::unique_ptr<sail::Tensor>> storage;
            std::map<std::string, sail::Tensor*> out_map;
            for (auto& n : out_names) {
                auto s = helmet.get_output_shape(graph, n);
                auto t = std::make_unique<sail::Tensor>(handle, s, helmet.get_output_dtype(graph, n), true, true);
                out_map[n] = t.get();
                storage.push_back(std::move(t));
            }
            helmet.process(graph, in_map, in_shapes, out_map);
            printf("  crop ratio=%.2f (h=%d):", ratio, ch);
            for (size_t k = 0; k < storage.size() && k < 4; ++k) {
                auto& t = *storage[k];
                t.sync_d2s();
                const float* p = static_cast<const float*>(t.sys_data());
                int n = 1;
                for (int v : helmet.get_output_shape(graph, out_names[k])) n *= v;
                float sum = 0;
                for (int j = 0; j < n; ++j) sum += std::exp(p[j]);
                printf(" head%zu[", k);
                for (int j = 0; j < n && j < 4; ++j) printf("%s%.2f", j ? "," : "", std::exp(p[j]) / (sum + 1e-9f));
                printf("]");
            }
            printf("\n");
        }
    }
    return 0;
}





