// Model/plugin probe: load a bmodel, run one image, print output shapes and
// top YOLOv5 / classifier scores. Used for RE validation, not shipped.
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

static std::string shape_str(const std::vector<int>& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i) o += ",";
        o += std::to_string(s[i]);
    }
    return o;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: %s model.bmodel image.jpg [alpha] [beta] [conf]\n", argv[0]);
        return 2;
    }
    float alpha = argc > 3 ? atof(argv[3]) : 1.0f;
    float beta = argc > 4 ? atof(argv[4]) : 0.0f;
    float conf = argc > 5 ? atof(argv[5]) : 0.25f;
    cv::Mat img = cv::imread(argv[2], cv::IMREAD_COLOR);
    if (img.empty()) { printf("image read failed\n"); return 3; }
    if (argc >= 10) {
        int cx=atoi(argv[6]), cy=atoi(argv[7]), cw=atoi(argv[8]), ch=atoi(argv[9]);
        cx=std::max(0,std::min(cx,img.cols-1)); cy=std::max(0,std::min(cy,img.rows-1));
        cw=std::min(cw,img.cols-cx); ch=std::min(ch,img.rows-cy);
        img = img(cv::Rect(cx,cy,cw,ch)).clone();
        printf("crop=%d,%d,%d,%d\\n",cx,cy,cw,ch);
    }
    sail::Handle handle(0);
    sail::Bmcv bmcv(handle);
    sail::Engine engine(argv[1], 0, sail::IOMode::SYSIO);
    auto graphs = engine.get_graph_names();
    if (graphs.empty()) { printf("no graph\n"); return 4; }
    std::string graph = graphs[0];
    auto in_names = engine.get_input_names(graph);
    std::string in_name = in_names[0];
    auto in_shape = engine.get_input_shape(graph, in_name);
    auto in_dtype = engine.get_input_dtype(graph, in_name);
    int nw = in_shape[3], nh = in_shape[2];
    printf("model=%s graph=%s input=%s shape=%s\n", argv[1], graph.c_str(),
           in_name.c_str(), shape_str(in_shape).c_str());
    auto out_names = engine.get_output_names(graph);
    std::map<std::string, std::vector<int>> out_shapes;
    for (auto& n : out_names) {
        auto s = engine.get_output_shape(graph, n);
        out_shapes[n] = s;
        printf("output %s shape=%s\n", n.c_str(), shape_str(s).c_str());
    }

    sail::BMImage src = sail::Bmcv::mat_to_bm_image(img);
    int w = src.width(), h = src.height();
    float r = std::min(static_cast<float>(nw) / w, static_cast<float>(nh) / h);
    int tw = std::max(1, static_cast<int>(std::round(w * r)));
    int th = std::max(1, static_cast<int>(std::round(h * r)));
    int tx = (nw - tw) / 2, ty = (nh - th) / 2;
    sail::PaddingAtrr pad;
    pad.set_stx(tx); pad.set_sty(ty); pad.set_w(tw); pad.set_h(th);
    pad.set_r(114); pad.set_g(114); pad.set_b(114);
    sail::BMImage resized = bmcv.crop_and_resize_padding(
        src, 0, 0, w, h, nw, nh, pad, bmcv_resize_algorithm::BMCV_INTER_LINEAR);
    sail::BMImage pre(handle, nh, nw, FORMAT_RGB_PLANAR, DATA_TYPE_EXT_FLOAT32);
    bmcv.convert_to(resized, pre,
                    std::make_tuple(std::make_pair(alpha, beta),
                                    std::make_pair(alpha, beta),
                                    std::make_pair(alpha, beta)));
    sail::Tensor input(handle, in_shape, in_dtype, true, true);
    bmcv.bm_image_to_tensor(pre, input);
    std::map<std::string, sail::Tensor*> in_map{{in_name, &input}};
    std::map<std::string, std::vector<int>> in_shapes{{in_name, in_shape}};
    std::vector<std::unique_ptr<sail::Tensor>> storage;
    std::map<std::string, sail::Tensor*> out_map;
    for (auto& n : out_names) {
        auto t = std::make_unique<sail::Tensor>(handle, out_shapes[n],
                                                engine.get_output_dtype(graph, n), true, true);
        out_map[n] = t.get();
        storage.push_back(std::move(t));
    }
    engine.process(graph, in_map, in_shapes, out_map);

    for (size_t oi = 0; oi < out_names.size(); ++oi) {
        auto& t = *storage[oi];
        t.sync_d2s();
        const float* p = static_cast<const float*>(t.sys_data());
        auto sh = out_shapes[out_names[oi]];
        int n = 1;
        for (int v : sh) n *= v;
        if (!p || n <= 0) continue;
        printf("tensor[%zu] %s n=%d", oi, out_names[oi].c_str(), n);
        if (const char* dp = std::getenv("DUMP_OUT")) { std::string path = dp; if (oi > 0) path += "." + std::to_string(oi); if (FILE* df = std::fopen(path.c_str(), "wb")) { std::fwrite(p, sizeof(float), (size_t)n, df); std::fclose(df); } }
        if (sh.size() == 3 && sh[2] == 10) {
            int boxes = sh[1];
            printf(" boxes=%d cols=10 yolo8-ish\\n", boxes);
            int imax=0; for(int i=0;i<boxes;i++) if(p[(size_t)i*10+9]>p[(size_t)imax*10+9]) imax=i;
            printf(" rawmax9[%d]:",imax); for(int k=0;k<10;k++) printf(" %.4f",p[(size_t)imax*10+k]); printf("\\n");
            struct C { float s; int c; float x1,y1,x2,y2; };
            std::vector<C> cs;
            for (int i=0;i<boxes;i++) {
                const float* q = p + (size_t)i*10;
                float best=-1e9f; int bc=0;
                for (int k=0;k<6;k++) if (q[4+k]>best) { best=q[4+k]; bc=k; }
                float s = 1.0f/(1.0f+std::exp(-best));
                if (s < 0.25f) continue;
                float cx=q[0], cy=q[1], w=q[2], h=q[3];
                cs.push_back({s,bc,cx-w/2,cy-h/2,cx+w/2,cy+h/2});
            }
            std::sort(cs.begin(),cs.end(),[](const C&a,const C&b){return a.s>b.s;});
            for (size_t i=0;i<cs.size()&&i<8;i++)
                printf("  cand score=%.4f class=%d box=%.3f,%.3f,%.3f,%.3f\\n",cs[i].s,cs[i].c,cs[i].x1,cs[i].y1,cs[i].x2,cs[i].y2);
        } else if (sh.size() == 3 && sh[2] == 2) {
            printf(" cols=2\\n");
            float mn=1e9f, mx=-1e9f;
            for (int i=0;i<n;i++){ mn=std::min(mn,p[i]); mx=std::max(mx,p[i]); }
            printf(" range=%.4f..%.4f\\n", mn, mx);
            int rows = std::min(6, sh[1]);
            for (int r=0;r<rows;r++){ printf(" row[%d]: %.4f %.4f\\n", r, p[(size_t)r*2], p[(size_t)r*2+1]); }
        } else if (sh.size() == 3 && sh[2] >= 5) {
            int boxes = sh[1], cols = sh[2];
            bool obj = (cols == 5 || cols == 9);
            printf(" boxes=%d cols=%d obj=%d\n", boxes, cols, obj ? 1 : 0);
            int imax=0; for(int i=0;i<boxes;i++) if(p[(size_t)i*cols+4]>p[(size_t)imax*cols+4]) imax=i;
            for (int z : {imax, 466, 467, 468}) { if(z<0||z>=boxes) continue; printf(" raw[%d]:",z); for(int k=0;k<cols;k++) printf(" %.3f", p[(size_t)z*cols+k]); printf("\\n"); }
            int nc = (cols == 5) ? 1 : (obj ? cols - 5 : cols - 4);
            float mo=0, mc[8]={0}; for(int i=0;i<boxes;i++){const float*q=p+(size_t)i*cols; mo=std::max(mo,q[4]); for(int c=0;c<nc&&c<8;c++) mc[c]=std::max(mc[c],q[5+c]);} printf(" max_obj=%.4f max_cls:",mo); for(int c=0;c<nc&&c<8;c++) printf(" %.4f",mc[c]); printf("\\n");
            struct Cand { float s; int c; float x1, y1, x2, y2; };
            std::vector<Cand> cs;
            static const float anchors[3][6] = {
                {10, 13, 16, 30, 33, 23}, {30, 61, 62, 45, 59, 119},
                {116, 90, 156, 198, 373, 326}};
            int strides[3] = {8, 16, 32};
            int starts[3] = {0, 0, 0}, cells[3] = {0, 0, 0};
            int total = 0;
            for (int l = 0; l < 3; ++l) {
                int nx = nw / strides[l], ny = nh / strides[l];
                cells[l] = nx * ny;
                starts[l] = total;
                total += cells[l];
            }
            for (int i = 0; i < boxes; ++i) {
                const float* q = p + static_cast<size_t>(i) * cols;
                int l = 0;
                while (l < 2 && i >= starts[l] + cells[l]) ++l;
                int local = i - starts[l];
                int nx = nw / strides[l];
                int a = local % 3;
                int cell = local / 3;
                int gx = cell % nx, gy = cell / nx;
                float objv = obj ? q[4] : 1.0f;
                (void)gx; (void)gy; (void)strides;
                float x = q[0];
                float y = q[1];
                float bw = q[2];
                float bh = q[3];
                for (int c = 0; c < nc; ++c) {
                    float s = objv * ((cols == 5) ? 1.0f : (obj ? q[5 + c] : q[4 + c]));
                    if (s < conf) continue;
                    cs.push_back({s, c, (x - bw / 2 - tx) / r, (y - bh / 2 - ty) / r,
                                  (x + bw / 2 - tx) / r, (y + bh / 2 - ty) / r});
                }
            }
            std::sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.s > b.s; });
            for (size_t i = 0; i < cs.size() && i < 20; ++i)
                printf("  #%zu score=%.4f class=%d box=%.0f,%.0f,%.0f,%.0f\n", i, cs[i].s,
                       cs[i].c, cs[i].x1, cs[i].y1, cs[i].x2, cs[i].y2);
        } else {
            printf("\n");
            int cls = n / (sh.size() >= 2 ? sh[0] : 1);
            if (cls > 0) {
                float sum = 0;
                for (int i = 0; i < cls; ++i) sum += std::exp(p[i]);
                printf("  probs:");
                for (int i = 0; i < cls; ++i) printf(" %.4f", std::exp(p[i]) / (sum + 1e-9f));
                printf("\n");
            }
        }
    }
    return 0;
}
