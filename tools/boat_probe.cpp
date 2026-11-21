// Boat life-jacket probe: run the boat detector on a person crop and print the
// per-class max scores (4 box + N class columns, YOLOv8 style).
// usage: boat_probe model.bmodel image.jpg conf x y w h [expand] [alpha] [beta]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <sail/cvwrapper.h>
#include <sail/engine.h>
#include <sail/tensor.h>

int main(int argc, char** argv) {
    if (argc < 8) {
        printf("usage: %s model image conf x y w h [expand] [alpha] [beta]\n", argv[0]);
        return 2;
    }
    cv::Mat img = cv::imread(argv[2], cv::IMREAD_COLOR);
    if (img.empty()) { printf("image read failed\n"); return 3; }
    float conf = (float)atof(argv[3]);
    float x = (float)atof(argv[4]), y = (float)atof(argv[5]);
    float w = (float)atof(argv[6]), h = (float)atof(argv[7]);
    float expand = argc > 8 ? (float)atof(argv[8]) : 1.10f;
    float alpha = argc > 9 ? (float)atof(argv[9]) : 1.0f / 255.0f;
    float beta = argc > 10 ? (float)atof(argv[10]) : 0.0f;
    // expand around the box centre
    float cx0 = x + w * 0.5f, cy0 = y + h * 0.5f;
    float nw0 = w * expand, nh0 = h * expand;
    int cx = std::max(0, (int)std::lround(cx0 - nw0 * 0.5f));
    int cy = std::max(0, (int)std::lround(cy0 - nh0 * 0.5f));
    int cw = std::min(img.cols - cx, (int)std::lround(nw0));
    int ch = std::min(img.rows - cy, (int)std::lround(nh0));
    if (cw < 8 || ch < 8) { printf("bad rect\n"); return 4; }

    sail::Handle handle(0);
    sail::Bmcv bmcv(handle);
    sail::Engine engine(argv[1], 0, sail::IOMode::SYSIO);
    std::string graph = engine.get_graph_names()[0];
    auto in_name = engine.get_input_names(graph)[0];
    auto in_shape = engine.get_input_shape(graph, in_name);
    int nw = in_shape[3], nh = in_shape[2];

    cv::Mat crop = img(cv::Rect(cx, cy, cw, ch));
    float r = std::min((float)nw / cw, (float)nh / ch);
    int tw = (int)std::lround(cw * r), th = (int)std::lround(ch * r);
    sail::PaddingAtrr pad;
    pad.set_stx((nw - tw) / 2); pad.set_sty((nh - th) / 2);
    pad.set_w(tw); pad.set_h(th);
    pad.set_r(114); pad.set_g(114); pad.set_b(114);
    sail::BMImage src = sail::Bmcv::mat_to_bm_image(crop);
    sail::BMImage rs = bmcv.crop_and_resize_padding(src, 0, 0, cw, ch, nw, nh, pad,
                                                    bmcv_resize_algorithm::BMCV_INTER_LINEAR);
    sail::BMImage pre(handle, nh, nw, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32);
    bmcv.convert_to(rs, pre, std::make_tuple(std::make_pair(alpha, beta),
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
    // Layout can be [1, anchors, 10] or [1, 10, anchors]; normalise to
    // (anchors, cols) with element (i, c) addressed per layout.
    const bool transposed = sh.size() >= 3 && sh[1] < sh[2];
    const int n = transposed ? sh[2] : sh[1];
    const int cols = transposed ? sh[1] : sh[2];
    printf("crop=%d,%d %dx%d expand=%.2f shape=%d,%d,%d transposed=%d anchors=%d cols=%d\n",
           cx, cy, cw, ch, expand, sh.size() > 0 ? sh[0] : 0,
           sh.size() > 1 ? sh[1] : 0, sh.size() > 2 ? sh[2] : 0, transposed ? 1 : 0, n,
           cols);
    auto prop = [](float v) { return (v >= 0.0f && v <= 1.0f) ? v : 1.0f / (1.0f + std::exp(-v)); };
    std::vector<float> best((size_t)std::max(0, cols - 4), 0.0f);
    int hits = 0;
    for (int i = 0; i < n; ++i) {
        bool any = false;
        for (int c = 4; c < cols; ++c) {
            const float v = transposed ? p[(size_t)c * n + i] : p[(size_t)i * cols + c];
            float s = prop(v);
            if (s > best[(size_t)(c - 4)]) best[(size_t)(c - 4)] = s;
            if (s >= conf) any = true;
        }
        if (any) ++hits;
    }
    printf("hits=%d classes:", hits);
    for (size_t c = 0; c < best.size(); ++c) printf(" c%zu=%.3f", c, best[c]);
    printf("\n");
    return 0;
}

