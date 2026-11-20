// VPP tile probe: verify crop+resize+padding matches a CPU crop for the
// production-style deep-mode tiles (4 rects of 0.6 W/H).
// usage: tile_probe model.bmodel image.jpg rx ry rw rh [alpha] [beta]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <sail/cvwrapper.h>
#include <sail/engine.h>
#include <sail/tensor.h>

int main(int argc, char** argv) {
    if (argc < 7) {
        printf("usage: %s model.bmodel image.jpg rx ry rw rh [alpha] [beta]\n", argv[0]);
        return 2;
    }
    float alpha = argc > 7 ? atof(argv[7]) : 1.0f / 255.0f;
    float beta = argc > 8 ? atof(argv[8]) : 0.0f;
    cv::Mat img = cv::imread(argv[2], cv::IMREAD_COLOR);
    if (img.empty()) { printf("image read failed\n"); return 3; }
    int rx = atoi(argv[3]), ry = atoi(argv[4]), rw = atoi(argv[5]), rh = atoi(argv[6]);
    printf("frame=%dx%d crop=%d,%d,%dx%d\n", img.cols, img.rows, rx, ry, rw, rh);

    sail::Handle handle(0);
    sail::Bmcv bmcv(handle);
    sail::BMImage src = sail::Bmcv::mat_to_bm_image(img);
    sail::Engine engine(argv[1], 0, sail::IOMode::SYSIO);
    auto graphs = engine.get_graph_names();
    std::string graph = graphs[0];
    auto in_names = engine.get_input_names(graph);
    std::string in_name = in_names[0];
    auto in_shape = engine.get_input_shape(graph, in_name);
    auto in_dtype = engine.get_input_dtype(graph, in_name);
    int nw = in_shape[3], nh = in_shape[2];
    printf("model input=%dx%d\n", nw, nh);

    float r = std::min(static_cast<float>(nw) / rw, static_cast<float>(nh) / rh);
    int tw = std::max(1, static_cast<int>(std::round(rw * r)));
    int th = std::max(1, static_cast<int>(std::round(rh * r)));
    int tx = (nw - tw) / 2, ty = (nh - th) / 2;
    sail::PaddingAtrr pad;
    pad.set_stx(tx); pad.set_sty(ty); pad.set_w(tw); pad.set_h(th);
    pad.set_r(114); pad.set_g(114); pad.set_b(114);
    sail::BMImage resized = bmcv.vpp_crop_and_resize_padding(
        src, rx, ry, rw, rh, nw, nh, pad, bmcv_resize_algorithm::BMCV_INTER_LINEAR);
    printf("vpp tw=%d th=%d tx=%d ty=%d\n", tw, th, tx, ty);
    sail::BMImage pre(handle, nh, nw, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32);
    bmcv.convert_to(resized, pre,
                    std::make_tuple(std::make_pair(alpha, beta),
                                    std::make_pair(alpha, beta),
                                    std::make_pair(alpha, beta)));
    sail::Tensor input(handle, in_shape, in_dtype, true, true);
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
    int boxes = sh.size() >= 3 ? sh[1] : 0;
    int cols = sh.size() >= 3 ? sh[2] : 0;
    float mo = -1.f;
    int imax = 0;
    for (int i = 0; i < boxes; ++i) {
        float v = p[(size_t)i * cols + 4];
        if (v > mo) { mo = v; imax = i; }
    }
    printf("max_obj=%.4f row[%d]:", mo, imax);
    for (int k = 0; k < cols && k < 9; ++k) printf(" %.3f", p[(size_t)imax * cols + k]);
    printf("\n");
    return 0;
}
