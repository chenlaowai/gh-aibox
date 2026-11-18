// Route-B compatibility layer: implements the subset of the Sophon `sail` API
// that this service uses, directly on top of bmcv_* / bmrt_* / sophon-ffmpeg.
//
// Why: the stock sail::Decoder decodes with libavcodec's *software* h264/hevc
// (yuvj420p in system memory) and then copies every frame to the device.  The
// production box instead decodes on the VPU (h264_bm/hevc_bm) and keeps frames
// in device memory.  This header keeps the existing call sites unchanged while
// switching the underlying stack:
//
//   sail::Handle/BMImage/Bmcv  -> bm_image_* + bmcv_*
//   sail::Engine/Tensor        -> bmrt_*
//   sail::Decoder              -> "h264_bm"/"hevc_bm" + NV12 device planes
//
// No libsail.so is linked; names are kept only so src/task.hpp etc. stay readable.
#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <bmlib_runtime.h>
#include <bmcv_api_ext.h>
#include <bmdef.h>
#include <bmruntime_interface.h>
}

namespace sail {

// ---------------------------------------------------------------- utilities
inline int dtype_size(bm_data_type_t t) {
    switch (t) {
        case BM_FLOAT32:
        case BM_INT32:
            return 4;
        case BM_FLOAT16:
        case BM_BFLOAT16:
        case BM_INT16:
            return 2;
        case BM_INT8:
        case BM_UINT8:
            return 1;
        default:
            return 1;
    }
}
inline int image_dtype_size(bm_image_data_format_ext t) {
    switch (t) {
        case DATA_TYPE_EXT_FLOAT32:
        case DATA_TYPE_EXT_4N_BYTE:
        case DATA_TYPE_EXT_4N_BYTE_SIGNED:
            return 4;
        case DATA_TYPE_EXT_FP16:
        case DATA_TYPE_EXT_BF16:
            return 2;
        default:
            return 1;
    }
}
inline int shape_count(const std::vector<int>& s) {
    int n = 1;
    for (int v : s) n *= v;
    return n;
}
inline int align_up(int v, int a) { return (v + a - 1) / a * a; }

class Handle {
public:
    explicit Handle(int dev_id = 0) {
        if (bm_dev_request(&h_, dev_id) != BM_SUCCESS) {
            h_ = nullptr;
            throw std::runtime_error("sail compat: bm_dev_request failed");
        }
    }
    ~Handle() {
        if (h_) bm_dev_free(h_);
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    bm_handle_t data() const { return h_; }
    int dev_id() const { return h_ ? bm_get_devid(h_) : 0; }

private:
    bm_handle_t h_ = nullptr;
};

class PaddingAtrr {
public:
    PaddingAtrr() = default;
    PaddingAtrr(int stx, int sty, int w, int h, int r, int g, int b)
        : stx_(stx), sty_(sty), w_(w), h_(h), r_(r), g_(g), b_(b) {}
    void set_stx(unsigned int v) { stx_ = static_cast<int>(v); }
    void set_sty(unsigned int v) { sty_ = static_cast<int>(v); }
    void set_w(unsigned int v) { w_ = static_cast<int>(v); }
    void set_h(unsigned int v) { h_ = static_cast<int>(v); }
    void set_r(unsigned int v) { r_ = static_cast<int>(v); }
    void set_g(unsigned int v) { g_ = static_cast<int>(v); }
    void set_b(unsigned int v) { b_ = static_cast<int>(v); }
    int stx() const { return stx_; }
    int sty() const { return sty_; }
    int w() const { return w_; }
    int h() const { return h_; }
    int r() const { return r_; }
    int g() const { return g_; }
    int b() const { return b_; }

private:
    int stx_ = 0, sty_ = 0, w_ = 0, h_ = 0, r_ = 0, g_ = 0, b_ = 0;
};

// ---------------------------------------------------------------- BMImage
// Move-only owner of a bm_image.  Device memory is allocated contiguously, like
// sail does, because bm_image_to_tensor needs one contiguous buffer.
class BMImage {
public:
    BMImage() = default;
    BMImage(Handle& h, int height, int width, bm_image_format_ext fmt,
            bm_image_data_format_ext dtype) {
        create(h, height, width, fmt, dtype, nullptr);
        allocate();
    }
    BMImage(Handle& h, int height, int width, bm_image_format_ext fmt,
            bm_image_data_format_ext dtype, int* stride) {
        create(h, height, width, fmt, dtype, stride);
        allocate();
    }
    explicit BMImage(bm_image&& img) {
        img_ = img;
        img = {};
        owns_ = false;
        created_ = true;
    }
    BMImage(BMImage&& other) noexcept { move_from(other); }
    BMImage& operator=(BMImage&& other) noexcept {
        if (this != &other) {
            destroy();
            move_from(other);
        }
        return *this;
    }
    BMImage(const BMImage&) = delete;
    BMImage& operator=(const BMImage&) = delete;
    ~BMImage() { destroy(); }

    bm_image& data() { return img_; }
    bm_image data() const { return img_; }
    int width() const { return img_.width; }
    int height() const { return img_.height; }
    bm_image_format_ext format() const { return img_.image_format; }
    bm_image_data_format_ext dtype() const { return img_.data_type; }
    bool is_created() const { return img_.image_private != nullptr; }
    bool empty() const { return !is_created() || img_.width <= 0 || img_.height <= 0; }

    void create(Handle& h, int height, int width, bm_image_format_ext fmt,
                bm_image_data_format_ext dtype, int* stride = nullptr) {
        if (is_created()) destroy();
        bm_status_t ret = bm_image_create(h.data(), height, width, fmt, dtype, &img_, stride);
        if (ret != BM_SUCCESS)
            throw std::runtime_error("sail compat: bm_image_create failed");
        created_ = true;
        owns_ = false;
    }
    void allocate() {
        if (!is_created() || owns_) return;
        // Heap mask 6 keeps user images on the same heap sail used; contiguous
        // allocation is required for bm_image_to_tensor.
        if (bm_image_alloc_contiguous_mem_heap_mask(1, &img_, 6) != BM_SUCCESS)
            throw std::runtime_error("sail compat: bm_image_alloc_contiguous_mem failed");
        owns_ = true;
    }
    void destroy() {
        if (!created_) return;
        if (owns_ && is_created()) {
            bm_image_free_contiguous_mem(1, &img_);
            owns_ = false;
        }
        if (is_created()) {
            bm_image_destroy(img_);
        }
        img_ = {};
        created_ = false;
    }
    void detach() {
        if (is_created() && bm_image_is_attached(img_)) bm_image_detach(img_);
    }
    void reset(int w, int h) {
        if (img_.width == w && img_.height == h) return;
        bm_image_format_ext fmt = img_.image_format;
        bm_image_data_format_ext dt = img_.data_type;
        bm_handle_t handle = bm_image_get_handle(&img_);
        destroy();
        if (!handle) return;
        bm_status_t ret = bm_image_create(handle, h, w, fmt, dt, &img_);
        if (ret != BM_SUCCESS) throw std::runtime_error("sail compat: bm_image reset failed");
        created_ = true;
        allocate();
    }
    void set_pts_dts(double pts, double dts) {
        pts_ = pts;
        dts_ = dts;
    }
    std::vector<double> get_pts_dts() const { return {pts_, dts_}; }

private:
    void move_from(BMImage& o) {
        img_ = o.img_;
        owns_ = o.owns_;
        created_ = o.created_;
        pts_ = o.pts_;
        dts_ = o.dts_;
        o.img_ = {};
        o.owns_ = false;
        o.created_ = false;
    }
    bm_image img_{};
    bool owns_ = false;
    bool created_ = false;
    double pts_ = 0, dts_ = 0;
};

class Tensor;

// ---------------------------------------------------------------- Bmcv
class Bmcv {
public:
    explicit Bmcv(Handle& h) : handle_(h.data()) {}

    bm_handle_t handle() const { return handle_; }

    // bm_image_create wants one stride per plane; planar BGR/RGB has 3 planes
    // that share the same width, so a uniform stride is correct.
    static BMImage make_image(int h, int w, bm_image_format_ext fmt,
                              bm_image_data_format_ext dtype, int stride) {
        BMImage out;
        int planes = (fmt == FORMAT_NV12 || fmt == FORMAT_YUV420P) ? 2
                     : (fmt == FORMAT_BGR_PLANAR || fmt == FORMAT_RGB_PLANAR) ? 3 : 1;
        int st[4] = {stride, stride, stride, stride};
        out.create(static_handle(), h, w, fmt, dtype, planes > 1 ? st : nullptr);
        out.allocate();
        return out;
    }

    int convert_format(BMImage& in, BMImage& out) {
        return convert_format(in, out, FORMAT_BGR_PLANAR);
    }
    int convert_format(BMImage& in, BMImage& out, bm_image_format_ext fmt) {
        if (!out.is_created() || out.width() != in.width() || out.height() != in.height() ||
            out.format() != fmt)
            out = make_image(in.height(), in.width(), fmt, in.dtype(), align_up(in.width() * image_dtype_size(in.dtype()), 64));
        return bmcv_image_vpp_convert(handle_, 1, in.data(), &out.data(), nullptr);
    }
    BMImage convert_format(BMImage& in, bm_image_format_ext fmt = FORMAT_BGR_PLANAR) {
        BMImage out;
        int rc = convert_format(in, out, fmt);
        if (rc != 0) throw std::runtime_error("sail compat: convert_format failed");
        return out;
    }
    int vpp_convert_format(BMImage& in, BMImage& out) { return convert_format(in, out); }
    int vpp_convert_format(BMImage& in, BMImage& out, bm_image_format_ext fmt) {
        return convert_format(in, out, fmt);
    }
    BMImage vpp_convert_format(BMImage& in, bm_image_format_ext fmt = FORMAT_BGR_PLANAR) {
        return convert_format(in, fmt);
    }

    // Resize/crop helpers.  vpp converts and resizes in one pass on the device.
    int crop_and_resize(BMImage& in, BMImage& out, int cx, int cy, int cw, int ch, int rw,
                        int rh, bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        if (!out.is_created() || out.width() != rw || out.height() != rh) {
            int ds = image_dtype_size(in.dtype());
            int stride = align_up(rw * ds, 64);
            // sail semantics: packed inputs produce a planar (BGR) output, so
            // downstream convert_to/draw see a stable layout.
            bm_image_format_ext ofmt = in.format();
            if (ofmt != FORMAT_BGR_PLANAR && ofmt != FORMAT_RGB_PLANAR && ofmt != FORMAT_GRAY)
                ofmt = FORMAT_BGR_PLANAR;
            out = make_image(rh, rw, ofmt, in.dtype(), stride);
        }
        bmcv_rect_t rect = {static_cast<unsigned short>(cx), static_cast<unsigned short>(cy),
                            static_cast<unsigned short>(cw), static_cast<unsigned short>(ch)};
        return bmcv_image_vpp_convert(handle_, 1, in.data(), &out.data(), &rect, alg);
    }
    BMImage crop_and_resize(BMImage& in, int cx, int cy, int cw, int ch, int rw, int rh,
                            bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        BMImage out;
        if (crop_and_resize(in, out, cx, cy, cw, ch, rw, rh, alg) != 0)
            throw std::runtime_error("sail compat: crop_and_resize failed");
        return out;
    }
    int vpp_crop_and_resize(BMImage& in, BMImage& out, int cx, int cy, int cw, int ch, int rw,
                            int rh, bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return crop_and_resize(in, out, cx, cy, cw, ch, rw, rh, alg);
    }
    BMImage vpp_crop_and_resize(BMImage& in, int cx, int cy, int cw, int ch, int rw, int rh,
                                bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return crop_and_resize(in, cx, cy, cw, ch, rw, rh, alg);
    }
    int crop_and_resize_padding(BMImage& in, BMImage& out, int cx, int cy, int cw, int ch,
                                    int rw, int rh, PaddingAtrr& pad,
                                    bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return vpp_crop_and_resize_padding(in, out, cx, cy, cw, ch, rw, rh, pad, alg);
    }
    BMImage crop_and_resize_padding(BMImage& in, int cx, int cy, int cw, int ch, int rw,
                                        int rh, PaddingAtrr& pad,
                                        bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return vpp_crop_and_resize_padding(in, cx, cy, cw, ch, rw, rh, pad, alg);
    }
    int vpp_crop_and_resize_padding(BMImage& in, BMImage& out, int cx, int cy, int cw, int ch,
                                    int rw, int rh, PaddingAtrr& pad,
                                    bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        if (!out.is_created() || out.width() != rw || out.height() != rh) {
            int ds = image_dtype_size(in.dtype());
            int stride = align_up(rw * ds, 64);
            // sail semantics: packed inputs produce a planar (BGR) output, so
            // downstream convert_to/draw see a stable layout.
            bm_image_format_ext ofmt = in.format();
            if (ofmt != FORMAT_BGR_PLANAR && ofmt != FORMAT_RGB_PLANAR && ofmt != FORMAT_GRAY)
                ofmt = FORMAT_BGR_PLANAR;
            out = make_image(rh, rw, ofmt, in.dtype(), stride);
        }
        bmcv_padding_atrr_t p{};
        p.dst_crop_stx = pad.stx();
        p.dst_crop_sty = pad.sty();
        p.dst_crop_w = pad.w();
        p.dst_crop_h = pad.h();
        p.padding_r = static_cast<unsigned char>(pad.r());
        p.padding_g = static_cast<unsigned char>(pad.g());
        p.padding_b = static_cast<unsigned char>(pad.b());
        if (p.dst_crop_w == 0) p.dst_crop_w = rw;
        if (p.dst_crop_h == 0) p.dst_crop_h = rh;
        if (p.dst_crop_w > rw) p.dst_crop_w = rw;
        if (p.dst_crop_h > rh) p.dst_crop_h = rh;
        bmcv_rect_t rect = {static_cast<unsigned short>(cx), static_cast<unsigned short>(cy),
                            static_cast<unsigned short>(cw), static_cast<unsigned short>(ch)};
        return bmcv_image_vpp_convert_padding(handle_, 1, in.data(), &out.data(), &p, &rect, alg);
    }
    BMImage vpp_crop_and_resize_padding(BMImage& in, int cx, int cy, int cw, int ch, int rw,
                                        int rh, PaddingAtrr& pad,
                                        bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        BMImage out;
        if (vpp_crop_and_resize_padding(in, out, cx, cy, cw, ch, rw, rh, pad, alg) != 0)
            throw std::runtime_error("sail compat: vpp_crop_and_resize_padding failed");
        return out;
    }
    int vpp_resize(BMImage& in, BMImage& out, int rw, int rh,
                   bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return crop_and_resize(in, out, 0, 0, in.width(), in.height(), rw, rh, alg);
    }
    BMImage vpp_resize(BMImage& in, int rw, int rh, bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return crop_and_resize(in, 0, 0, in.width(), in.height(), rw, rh, alg);
    }
    int resize(BMImage& in, BMImage& out, int rw, int rh,
               bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return vpp_resize(in, out, rw, rh, alg);
    }
    BMImage resize(BMImage& in, int rw, int rh, bmcv_resize_algorithm alg = BMCV_INTER_NEAREST) {
        return vpp_resize(in, rw, rh, alg);
    }

    int convert_to(BMImage& in, BMImage& out,
                   const std::tuple<std::pair<float, float>, std::pair<float, float>,
                                    std::pair<float, float>>& ab) {
        if (!out.is_created()) {
            int ds = image_dtype_size(DATA_TYPE_EXT_FLOAT32);
            (void)ds;
            int stride = align_up(in.width() * 4, 64);
            out = make_image(in.height(), in.width(), in.format(), DATA_TYPE_EXT_FLOAT32, stride);
        }
        bmcv_convert_to_attr attr{};
        attr.alpha_0 = std::get<0>(ab).first;
        attr.beta_0 = std::get<0>(ab).second;
        attr.alpha_1 = std::get<1>(ab).first;
        attr.beta_1 = std::get<1>(ab).second;
        attr.alpha_2 = std::get<2>(ab).first;
        attr.beta_2 = std::get<2>(ab).second;
        return bmcv_image_convert_to(handle_, 1, attr, &in.data(), &out.data());
    }
    BMImage convert_to(BMImage& in,
                       const std::tuple<std::pair<float, float>, std::pair<float, float>,
                                        std::pair<float, float>>& ab) {
        BMImage out;
        int rc = convert_to(in, out, ab);
        if (rc != 0) throw std::runtime_error("sail compat: convert_to failed");
        return out;
    }

    int image_copy_to(BMImage& in, BMImage& out, int x = 0, int y = 0) {
        if (!out.is_created()) {
            out = make_image(in.height(), in.width(), in.format(), in.dtype(),
                             align_up(in.width() * image_dtype_size(in.dtype()), 64));
        }
        bmcv_copy_to_atrr_t attr{};
        attr.start_x = x;
        attr.start_y = y;
        attr.if_padding = 0;
        return bmcv_image_copy_to(handle_, attr, in.data(), out.data());
    }

    int rectangle(const BMImage& img, int x, int y, int w, int h,
                  const std::tuple<int, int, int>& color, int thickness = 1) {
        bm_image bi = const_cast<BMImage&>(img).data();
        bmcv_rect_t r = {static_cast<unsigned short>(std::max(0, x)),
                         static_cast<unsigned short>(std::max(0, y)),
                         static_cast<unsigned short>(std::max(1, w)),
                         static_cast<unsigned short>(std::max(1, h))};
        return bmcv_image_draw_rectangle(handle_, bi, 1, &r, std::max(1, thickness),
                                         static_cast<unsigned char>(std::get<2>(color)),
                                         static_cast<unsigned char>(std::get<1>(color)),
                                         static_cast<unsigned char>(std::get<0>(color)));
    }
    int rectangle(const bm_image& img, int x, int y, int w, int h,
                  const std::tuple<int, int, int>& color, int thickness = 1) {
        bm_image bi = img;
        BMImage tmp(std::move(bi));
        return rectangle(tmp, x, y, w, h, color, thickness);
    }
    int fill_rectangle(const BMImage& img, int x, int y, int w, int h,
                       const std::tuple<int, int, int>& color) {
        bm_image bi = const_cast<BMImage&>(img).data();
        bmcv_rect_t r = {static_cast<unsigned short>(std::max(0, x)),
                         static_cast<unsigned short>(std::max(0, y)),
                         static_cast<unsigned short>(std::max(1, w)),
                         static_cast<unsigned short>(std::max(1, h))};
        return bmcv_image_fill_rectangle(handle_, bi, 1, &r,
                                         static_cast<unsigned char>(std::get<2>(color)),
                                         static_cast<unsigned char>(std::get<1>(color)),
                                         static_cast<unsigned char>(std::get<0>(color)));
    }
    int polylines(BMImage& img, std::vector<std::vector<std::pair<int, int>>>& pts,
                  bool closed, std::tuple<unsigned char, unsigned char, unsigned char> color,
                  int thickness = 1, int shift = 0) {
        (void)shift;
        int rc = 0;
        for (auto& poly : pts) {
            if (poly.size() < 2) continue;
            for (size_t i = 0; i + 1 < poly.size(); ++i) {
                bmcv_rect_t r{};
                int x0 = poly[i].first, y0 = poly[i].second;
                int x1 = poly[i + 1].first, y1 = poly[i + 1].second;
                // bmcv only has rectangles: approximate a segment with a thin rect
                if (x0 == x1) {
                    r = {static_cast<unsigned short>(x0),
                         static_cast<unsigned short>(std::min(y0, y1)),
                         static_cast<unsigned short>(std::max(1, thickness)),
                         static_cast<unsigned short>(std::abs(y1 - y0))};
                } else if (y0 == y1) {
                    r = {static_cast<unsigned short>(std::min(x0, x1)),
                         static_cast<unsigned short>(y0),
                         static_cast<unsigned short>(std::abs(x1 - x0)),
                         static_cast<unsigned short>(std::max(1, thickness))};
                } else {
                    continue;  // diagonal edges are drawn on the CPU path only
                }
                rc |= bmcv_image_draw_rectangle(handle_, img.data(), 1, &r,
                                                std::max(1, thickness), std::get<2>(color),
                                                std::get<1>(color), std::get<0>(color));
            }
            if (closed && poly.size() > 2) {
                bmcv_rect_t r{};
                int x0 = poly.back().first, y0 = poly.back().second;
                int x1 = poly.front().first, y1 = poly.front().second;
                if (x0 == x1 || y0 == y1) {
                    r = {static_cast<unsigned short>(std::min(x0, x1)),
                         static_cast<unsigned short>(std::min(y0, y1)),
                         static_cast<unsigned short>(std::max(1, std::abs(x1 - x0))),
                         static_cast<unsigned short>(std::max(1, std::abs(y1 - y0)))};
                    rc |= bmcv_image_draw_rectangle(handle_, img.data(), 1, &r,
                                                    std::max(1, thickness), std::get<2>(color),
                                                    std::get<1>(color), std::get<0>(color));
                }
            }
        }
        return rc;
    }

    bool imencode(std::string& ext, BMImage& img, std::vector<unsigned char>& buf) {
        cv::Mat mat;
        if (bm_image_to_mat(img, mat) != 0 || mat.empty()) return false;
        return cv::imencode(ext, mat, buf);
    }
    BMImage imdecode(const void* data, size_t size) {
        cv::Mat raw(1, static_cast<int>(size), CV_8UC1, const_cast<void*>(data));
        cv::Mat mat = cv::imdecode(raw, cv::IMREAD_COLOR);
        if (mat.empty()) return BMImage();
        return mat_to_bm_image(mat);
    }
    BMImage imread(const std::string& path) {
        cv::Mat mat = cv::imread(path, cv::IMREAD_COLOR);
        if (mat.empty()) return BMImage();
        return mat_to_bm_image(mat);
    }

    // Copy one plane from device memory.  bm_image_get_device_mem returns a
    // descriptor whose size can be the whole contiguous allocation (larger than
    // one stride*rows plane), so never use bm_memcpy_d2s into a plane-sized
    // buffer: that overruns the host heap (it crashed render_jpeg).
    static int copy_plane(bm_handle_t h, bm_device_mem_t mem, int stride, int rows,
                          std::vector<uint8_t>& out) {
        size_t want = static_cast<size_t>(stride) * rows;
        size_t dev = bm_mem_get_device_size(mem);
        size_t n = std::min(want, dev);
        out.assign(n, 0);
        if (n && bm_memcpy_d2s_partial(h, out.data(), mem, static_cast<unsigned int>(n)) !=
                     BM_SUCCESS)
            return -1;
        return 0;
    }

    static int bm_image_to_mat(BMImage& img, cv::Mat& out) {
        bm_image& b = img.data();
        if (!b.image_private || b.width <= 0 || b.height <= 0) return -1;
        bm_handle_t h = bm_image_get_handle(&b);
        int planes = bm_image_get_plane_num(b);
        std::vector<bm_device_mem_t> mem(std::max(1, planes));
        std::vector<int> stride(std::max(1, planes), 0);
        if (bm_image_get_device_mem(b, mem.data()) != BM_SUCCESS) return -1;
        if (bm_image_get_stride(b, stride.data()) != BM_SUCCESS) return -1;
        int h_ = b.height, w_ = b.width;
        switch (b.image_format) {
            case FORMAT_BGR_PACKED:
            case FORMAT_RGB_PACKED: {
                std::vector<uint8_t> tmp;
                if (copy_plane(h, mem[0], stride[0], h_, tmp) != 0) return -1;
                int rows = static_cast<int>(tmp.size() / std::max(1, stride[0]));
                out.create(h_, w_, CV_8UC3);
                for (int y = 0; y < rows; ++y)
                    std::memcpy(out.ptr(y), tmp.data() + static_cast<size_t>(y) * stride[0], w_ * 3);
                if (rows < h_) out.rowRange(rows, h_).setTo(cv::Scalar::all(0));
                if (b.image_format == FORMAT_RGB_PACKED) cv::cvtColor(out, out, cv::COLOR_RGB2BGR);
                return 0;
            }
            case FORMAT_BGR_PLANAR:
            case FORMAT_RGB_PLANAR: {
                std::vector<cv::Mat> ch(3);
                for (int p = 0; p < 3; ++p) {
                    std::vector<uint8_t> tmp;
                    if (copy_plane(h, mem[std::min(p, planes - 1)], stride[std::min(p, planes - 1)],
                                   h_, tmp) != 0)
                        return -1;
                    int st = stride[std::min(p, planes - 1)];
                    int rows = static_cast<int>(tmp.size() / std::max(1, st));
                    ch[p].create(h_, w_, CV_8UC1);
                    for (int y = 0; y < rows; ++y)
                        std::memcpy(ch[p].ptr(y), tmp.data() + static_cast<size_t>(y) * st, w_);
                    if (rows < h_) ch[p].rowRange(rows, h_).setTo(cv::Scalar::all(0));
                }
                // bmcv planar order is B,G,R
                if (b.image_format == FORMAT_BGR_PLANAR)
                    cv::merge(std::vector<cv::Mat>{ch[0], ch[1], ch[2]}, out);
                else
                    cv::merge(std::vector<cv::Mat>{ch[2], ch[1], ch[0]}, out);
                return 0;
            }
            case FORMAT_GRAY: {
                std::vector<uint8_t> tmp;
                if (copy_plane(h, mem[0], stride[0], h_, tmp) != 0) return -1;
                int rows = static_cast<int>(tmp.size() / std::max(1, stride[0]));
                out.create(h_, w_, CV_8UC1);
                for (int y = 0; y < rows; ++y)
                    std::memcpy(out.ptr(y), tmp.data() + static_cast<size_t>(y) * stride[0], w_);
                if (rows < h_) out.rowRange(rows, h_).setTo(cv::Scalar::all(0));
                return 0;
            }
            default:
                return -1;
        }
    }
    static cv::Mat bm_image_to_mat(BMImage& img) {
        cv::Mat m;
        bm_image_to_mat(img, m);
        return m;
    }
    static int mat_to_bm_image(cv::Mat& mat, BMImage& img) {
        if (mat.empty()) return -1;
        Handle& h = static_handle();
        img.create(h, mat.rows, mat.cols, FORMAT_BGR_PACKED, DATA_TYPE_EXT_1N_BYTE);
        img.allocate();
        bm_device_mem_t mem;
        if (bm_image_get_device_mem(img.data(), &mem) != BM_SUCCESS) return -1;
        if (!mat.isContinuous()) mat = mat.clone();
        size_t n = std::min<size_t>(static_cast<size_t>(mat.total() * mat.elemSize()),
                                bm_mem_get_device_size(mem));
        return bm_memcpy_s2d_partial(h.data(), mem, mat.data, static_cast<unsigned int>(n)) ==
                       BM_SUCCESS
                   ? 0
                   : -1;
    }
    static BMImage mat_to_bm_image(cv::Mat& mat) {
        BMImage img;
        if (mat_to_bm_image(mat, img) != 0) return BMImage();
        return img;
    }

    void bm_image_to_tensor(BMImage& img, Tensor& tensor);

    // bmcv needs a Handle object for helpers that create images; keep one
    // process-wide (one device).
    static Handle& static_handle() {
        static Handle h(0);
        return h;
    }

private:
    Handle* handle_obj() { return &static_handle(); }
    bm_handle_t handle_ = nullptr;
};

// ---------------------------------------------------------------- Tensor
class Tensor {
public:
    Tensor() = default;
    Tensor(Handle& h, const std::vector<int>& shape, bm_data_type_t dtype, bool sys = true,
           bool dev = true)
        : handle_(h.data()), shape_(shape), dtype_(dtype) {
        allocate(sys, dev);
    }
    Tensor(Tensor&& o) noexcept { move_from(o); }
    Tensor& operator=(Tensor&& o) noexcept {
        if (this != &o) {
            free_all();
            move_from(o);
        }
        return *this;
    }
    Tensor(const Tensor&) = delete;
    Tensor& operator=(const Tensor&) = delete;
    ~Tensor() { free_all(); }

    void allocate(bool sys, bool dev) {
        size_t bytes = static_cast<size_t>(shape_count(shape_)) * dtype_size(dtype_);
        if (dev) {
            if (bm_malloc_device_byte(handle_, &dev_, static_cast<unsigned int>(bytes)) !=
                BM_SUCCESS)
                throw std::runtime_error("sail compat: bm_malloc_device_byte failed");
            has_dev_ = true;
        }
        if (sys) {
            sys_.resize(bytes);
            has_sys_ = true;
        }
    }
    void reset_dev_data(bm_device_mem_t mem) {
        // Point this tensor at external device memory (bm_image) and drop any
        // allocation we made ourselves.
        if (has_dev_) {
            bm_free_device(handle_, dev_);
            has_dev_ = false;
        }
        dev_ = mem;
        external_dev_ = true;
    }
    void reset(const std::vector<int>& shape, bm_data_type_t dtype, bool alloc = true) {
        if (shape == shape_ && dtype == dtype_ && has_dev_) return;
        free_all();
        shape_ = shape;
        dtype_ = dtype;
        if (alloc) allocate(true, true);
        else {
            sys_.resize(static_cast<size_t>(shape_count(shape_)) * dtype_size(dtype_));
            has_sys_ = true;
        }
    }
    void sync_d2s() {
        if (!has_dev_) return;
        size_t bytes = static_cast<size_t>(shape_count(shape_)) * dtype_size(dtype_);
        if (!has_sys_) {
            sys_.resize(bytes);
            has_sys_ = true;
        }
        size_t dev_size = bm_mem_get_device_size(dev_);
        size_t n = std::min(bytes, dev_size);
        if (n) bm_memcpy_d2s_partial(handle_, sys_.data(), dev_, static_cast<unsigned int>(n));
    }
    void* sys_data() { return has_sys_ ? sys_.data() : nullptr; }
    bm_device_mem_t device_mem() const { return dev_; }
    const std::vector<int>& shape() const { return shape_; }
    bm_data_type_t dtype() const { return dtype_; }
    bm_handle_t handle() const { return handle_; }

private:
    void free_all() {
        if (has_dev_ && !external_dev_) bm_free_device(handle_, dev_);
        has_dev_ = false;
        external_dev_ = false;
        has_sys_ = false;
        sys_.clear();
    }
    void move_from(Tensor& o) {
        handle_ = o.handle_;
        shape_ = o.shape_;
        dtype_ = o.dtype_;
        dev_ = o.dev_;
        external_dev_ = o.external_dev_;
        has_dev_ = o.has_dev_;
        sys_ = std::move(o.sys_);
        has_sys_ = o.has_sys_;
        o.has_dev_ = false;
        o.external_dev_ = false;
        o.has_sys_ = false;
    }
    bm_handle_t handle_ = nullptr;
    std::vector<int> shape_;
    bm_data_type_t dtype_ = BM_FLOAT32;
    bm_device_mem_t dev_{};
    bool has_dev_ = false;
    bool external_dev_ = false;
    std::vector<uint8_t> sys_;
    bool has_sys_ = false;
};

inline void Bmcv::bm_image_to_tensor(BMImage& img, Tensor& tensor) {
    bm_device_mem_t mem;
    if (bm_image_get_device_mem(img.data(), &mem) != BM_SUCCESS)
        throw std::runtime_error("sail compat: bm_image_get_device_mem failed");
    int h = img.height(), w = img.width();
    std::vector<int> shape;
    switch (img.format()) {
        case FORMAT_BGR_PLANAR:
        case FORMAT_RGB_PLANAR:
            shape = {1, 3, h, w};
            break;
        case FORMAT_BGR_PACKED:
        case FORMAT_RGB_PACKED:
            shape = {1, h, w, 3};
            break;
        case FORMAT_GRAY:
            shape = {1, 1, h, w};
            break;
        default:
            throw std::runtime_error("sail compat: image format not supported by bm_image_to_tensor");
    }
    bm_data_type_t dt = BM_UINT8;
    if (img.dtype() == DATA_TYPE_EXT_FLOAT32) dt = BM_FLOAT32;
    tensor.reset(shape, dt, /*alloc=*/false);
    tensor.reset_dev_data(mem);
}

// ---------------------------------------------------------------- Engine
enum class IOMode { SYSIO, DEVIO };

class Engine {
public:
    Engine(const std::string& path, int dev_id = 0, IOMode mode = IOMode::SYSIO) {
        (void)mode;
        if (bm_dev_request(&handle_, dev_id) != BM_SUCCESS)
            throw std::runtime_error("sail compat: engine bm_dev_request failed");
        rt_ = bmrt_create(handle_);
        if (!rt_) throw std::runtime_error("sail compat: bmrt_create failed");
        if (!bmrt_load_bmodel(rt_, path.c_str()))
            throw std::runtime_error("sail compat: load bmodel failed: " + path);
        int n = bmrt_get_network_number(rt_);
        for (int i = 0; i < n; ++i) graphs_.push_back(bmrt_get_network_name(rt_, i));
    }
    ~Engine() {
        if (rt_) bmrt_destroy(rt_);
        if (handle_) bm_dev_free(handle_);
    }
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    std::vector<std::string> get_graph_names() const { return graphs_; }

    const bm_net_info_t* info(const std::string& graph) {
        return bmrt_get_network_info(rt_, graph.c_str());
    }
    std::vector<std::string> get_input_names(const std::string& graph) {
        const bm_net_info_t* ni = info(graph);
        std::vector<std::string> out;
        if (!ni) return out;
        for (int i = 0; i < ni->input_num; ++i) out.emplace_back(ni->input_names[i]);
        return out;
    }
    std::vector<std::string> get_output_names(const std::string& graph) {
        const bm_net_info_t* ni = info(graph);
        std::vector<std::string> out;
        if (!ni) return out;
        for (int i = 0; i < ni->output_num; ++i) out.emplace_back(ni->output_names[i]);
        return out;
    }
    std::vector<int> get_input_shape(const std::string& graph, const std::string& name) {
        return shape_of(graph, name, true);
    }
    std::vector<int> get_output_shape(const std::string& graph, const std::string& name) {
        return shape_of(graph, name, false);
    }
    bm_data_type_t get_input_dtype(const std::string& graph, const std::string& name) {
        const bm_net_info_t* ni = info(graph);
        if (!ni) return BM_FLOAT32;
        for (int i = 0; i < ni->input_num; ++i)
            if (name == ni->input_names[i]) return ni->input_dtypes[i];
        return BM_FLOAT32;
    }
    bm_data_type_t get_output_dtype(const std::string& graph, const std::string& name) {
        const bm_net_info_t* ni = info(graph);
        if (!ni) return BM_FLOAT32;
        for (int i = 0; i < ni->output_num; ++i)
            if (name == ni->output_names[i]) return ni->output_dtypes[i];
        return BM_FLOAT32;
    }

    void process(const std::string& graph, std::map<std::string, Tensor*>& inputs,
                 std::map<std::string, std::vector<int>>& input_shapes,
                 std::map<std::string, Tensor*>& outputs) {
        (void)input_shapes;
        const bm_net_info_t* ni = info(graph);
        if (!ni) throw std::runtime_error("sail compat: graph not found");
        std::vector<bm_tensor_t> in(ni->input_num), out(ni->output_num);
        for (int i = 0; i < ni->input_num; ++i) {
            auto it = inputs.find(ni->input_names[i]);
            if (it == inputs.end() || !it->second)
                throw std::runtime_error("sail compat: missing input " +
                                         std::string(ni->input_names[i]));
            Tensor* t = it->second;
            in[i].dtype = t->dtype();
            bmrt_shape(&in[i].shape, t->shape().data(),
                       static_cast<int>(t->shape().size()));
            in[i].device_mem = t->device_mem();
            in[i].st_mode = BM_STORE_1N;
        }
        for (int i = 0; i < ni->output_num; ++i) {
            auto it = outputs.find(ni->output_names[i]);
            if (it == outputs.end() || !it->second)
                throw std::runtime_error("sail compat: missing output " +
                                         std::string(ni->output_names[i]));
            Tensor* t = it->second;
            out[i].dtype = t->dtype();
            bmrt_shape(&out[i].shape, t->shape().data(),
                       static_cast<int>(t->shape().size()));
            out[i].device_mem = t->device_mem();
            out[i].st_mode = BM_STORE_1N;
        }
        if (!bmrt_launch_tensor_ex(rt_, graph.c_str(), in.data(), ni->input_num, out.data(),
                                   ni->output_num, true, false))
            throw std::runtime_error("sail compat: bmrt_launch_tensor_ex failed");
    }

private:
    std::vector<int> shape_of(const std::string& graph, const std::string& name, bool input) {
        std::vector<int> out;
        const bm_net_info_t* ni = info(graph);
        if (!ni) return out;
        const bm_shape_t* s = nullptr;
        if (input) {
            for (int i = 0; i < ni->input_num; ++i)
                if (name == ni->input_names[i]) s = &ni->stages[0].input_shapes[i];
        } else {
            for (int i = 0; i < ni->output_num; ++i)
                if (name == ni->output_names[i]) s = &ni->stages[0].output_shapes[i];
        }
        if (!s) return out;
        for (int i = 0; i < s->num_dims; ++i) out.push_back(s->dims[i]);
        return out;
    }
    bm_handle_t handle_ = nullptr;
    void* rt_ = nullptr;
    std::vector<std::string> graphs_;
};

// ---------------------------------------------------------------- Decoder
inline void set_decoder_env(const std::string& key, const std::string& value) {
    setenv(key.c_str(), value.c_str(), 1);
}

class Decoder {
public:
    Decoder(const std::string& url, bool compressed, int tpu_id)
        : url_(url), compressed_(compressed), tpu_id_(tpu_id) {
        open();
    }
    ~Decoder() { release(); }
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    bool is_opened() const { return opened_; }
    bool is_eof() const { return eof_; }

    // Mirrors sail: {1, 3, h, w}
    std::vector<int> get_frame_shape() {
        std::lock_guard<std::mutex> lk(mu_);
        return {1, 3, height_, width_};
    }

    int read(Handle& h, BMImage& image) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!opened_ || !cc_) return kReadErr;
        for (int guard = 0; guard < 200; ++guard) {
            int ret = avcodec_receive_frame(cc_, frame_);
            if (ret == 0) {
                int rc = frame_to_image(h, image);
                av_frame_unref(frame_);
                if (rc != 0) continue;
                return 0;
            }
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                if (ret == AVERROR_EOF) eof_ = true;
                return kReadErr;
            }
            if (eof_) return kDecEof;
            if (!read_and_send_packet()) return eof_ ? kDecEof : kReadErr;
        }
        return kReadErr;
    }

    BMImage read(Handle& h) {
        BMImage img;
        if (read(h, img) != 0) return BMImage();
        return img;
    }

    void enable_dump(int /*seconds*/) {}
    void disable_dump() {}
    int dump(int /*pre*/, int /*post*/, std::string& /*path*/) { return -1; }

    int reconnect() {
        std::lock_guard<std::mutex> lk(mu_);
        release_locked();
        open_locked();
        return opened_ ? 0 : kOpenErr;
    }

private:
    enum { kReadErr = -6, kDecEof = -7, kOpenErr = -8 };

    static int interrupt_cb(void* opaque) {
        auto* self = static_cast<Decoder*>(opaque);
        if (!self) return 0;
        if (self->deadline_ms_ > 0 && now_ms() > self->deadline_ms_) return 1;
        return 0;
    }
    static int64_t now_ms() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
    }

    void open() {
        std::lock_guard<std::mutex> lk(mu_);
        open_locked();
    }

    void open_locked() {
        opened_ = false;
        eof_ = false;
        avformat_network_init();
        AVDictionary* opts = nullptr;
        const char* transport = getenv("rtsp_transport");
        av_dict_set(&opts, "rtsp_transport", transport ? transport : "tcp", 0);
        const char* stimeout = getenv("stimeout");
        av_dict_set(&opts, "stimeout", stimeout ? stimeout : "10000000", 0);
        av_dict_set(&opts, "buffer_size", "4194304", 0);
        fmt_ = avformat_alloc_context();
        fmt_->interrupt_callback.callback = &Decoder::interrupt_cb;
        fmt_->interrupt_callback.opaque = this;
        deadline_ms_ = now_ms() + 10000;
        if (avformat_open_input(&fmt_, url_.c_str(), nullptr, &opts) < 0) {
            av_dict_free(&opts);
            return;
        }
        av_dict_free(&opts);
        if (avformat_find_stream_info(fmt_, nullptr) < 0) return;
        int vs = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (vs < 0) return;
        vs_ = vs;
        AVStream* st = fmt_->streams[vs];
        const char* want = st->codecpar->codec_id == AV_CODEC_ID_HEVC ? "hevc_bm" : "h264_bm";
        const AVCodec* dec = avcodec_find_decoder_by_name(want);
        if (!dec) dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) return;
        cc_ = avcodec_alloc_context3(dec);
        if (!cc_) return;
        avcodec_parameters_to_context(cc_, st->codecpar);
        cc_->thread_count = 1;
        if (avcodec_open2(cc_, dec, nullptr) < 0) {
            avcodec_free_context(&cc_);
            return;
        }
        pkt_ = av_packet_alloc();
        frame_ = av_frame_alloc();
        width_ = cc_->width;
        height_ = cc_->height;
        opened_ = true;
    }

    bool read_and_send_packet() {
        for (int i = 0; i < 64; ++i) {
            deadline_ms_ = now_ms() + 10000;
            int ret = av_read_frame(fmt_, pkt_);
            if (ret < 0) {
                eof_ = true;
                av_packet_unref(pkt_);
                return false;
            }
            if (pkt_->stream_index != vs_) {
                av_packet_unref(pkt_);
                continue;
            }
            ret = avcodec_send_packet(cc_, pkt_);
            av_packet_unref(pkt_);
            return ret >= 0;
        }
        return false;
    }

    int frame_to_image(Handle& h, BMImage& image) {
        AVFrame* f = frame_;
        if (f->width <= 0 || f->height <= 0) return -1;
        int w = f->width, hh = f->height;
        width_ = w;
        height_ = hh;
        // VPU frames: NV12 with device plane addresses in data[4]/[5].
        if (f->data[4] && f->format == AV_PIX_FMT_NV12) {
            int stride[2] = {f->linesize[4], f->linesize[5]};
            if (!nv12_created_ || nv12_.width() != w || nv12_.height() != hh) {
                nv12_.create(h, hh, w, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, stride);
                nv12_created_ = true;
            }
            bm_device_mem_t planes[2];
            planes[0] = bm_mem_from_device((unsigned long long)f->data[4],
                                           static_cast<unsigned long long>(hh) * stride[0]);
            planes[1] = bm_mem_from_device((unsigned long long)f->data[5],
                                           static_cast<unsigned long long>(hh / 2) * stride[1]);
            if (bm_image_attach(nv12_.data(), planes) != BM_SUCCESS) return -1;
            if (!image.is_created() || image.width() != w || image.height() != hh) {
                image.create(h, hh, w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE);
                image.allocate();
            }
            int rc = bmcv_image_vpp_convert(h.data(), 1, nv12_.data(), &image.data(), nullptr);
            nv12_.detach();
            if (rc != BM_SUCCESS) return -1;
            return 0;
        }
        // Fallback (software decode of odd codecs): host NV12/YUV420P planes.
        if (!image.is_created() || image.width() != w || image.height() != hh) {
            image.create(h, hh, w, FORMAT_BGR_PLANAR, DATA_TYPE_EXT_1N_BYTE);
            image.allocate();
        }
        if (f->format == AV_PIX_FMT_NV12 || f->format == AV_PIX_FMT_YUV420P ||
            f->format == AV_PIX_FMT_YUVJ420P) {
            bool nv12 = (f->format == AV_PIX_FMT_NV12);
            bm_image_format_ext fmt = nv12 ? FORMAT_NV12 : FORMAT_YUV420P;
            bm_image tmp;
            int stride[3] = {f->linesize[0], f->linesize[1], f->linesize[2]};
            if (bm_image_create(h.data(), hh, w, fmt, DATA_TYPE_EXT_1N_BYTE, &tmp, stride) !=
                BM_SUCCESS)
                return -1;
            if (bm_image_alloc_dev_mem(tmp, 1) != BM_SUCCESS) {
                bm_image_destroy(tmp);
                return -1;
            }
            void* src[3] = {f->data[0], f->data[1], f->data[2]};
            bm_image_copy_host_to_device(tmp, src);
            int rc = bmcv_image_vpp_convert(h.data(), 1, tmp, &image.data(), nullptr);
            bm_image_destroy(tmp);
            return rc == BM_SUCCESS ? 0 : -1;
        }
        return -1;
    }

    void release() {
        std::lock_guard<std::mutex> lk(mu_);
        release_locked();
    }
    void release_locked() {
        if (pkt_) av_packet_free(&pkt_);
        if (frame_) av_frame_free(&frame_);
        if (cc_) avcodec_free_context(&cc_);
        if (fmt_) avformat_close_input(&fmt_);
        opened_ = false;
        eof_ = false;
    }

    std::mutex mu_;
    std::string url_;
    bool compressed_ = false;
    int tpu_id_ = 0;
    AVFormatContext* fmt_ = nullptr;
    AVCodecContext* cc_ = nullptr;
    AVPacket* pkt_ = nullptr;
    AVFrame* frame_ = nullptr;
    int vs_ = -1;
    int width_ = 0, height_ = 0;
    bool opened_ = false;
    bool eof_ = false;
    int64_t deadline_ms_ = 0;
    BMImage nv12_;
    bool nv12_created_ = false;
};

// ---------------------------------------------------------------- MultiDecoder
// Grouped decoder used when AIDETECT_DECODE_MODE=multi.  Each channel owns one
// Decoder thread and publishes the newest frame; read() copies it into the
// caller's image.  (Production runs "single", where every task has its own
// Decoder, so this path is a compatibility fallback.)
class MultiDecoder {
public:
    MultiDecoder(int max_channels, int device_id, int tpu_id, bool /*attach*/ = false)
        : max_(std::max(1, max_channels)), device_id_(device_id), tpu_id_(tpu_id) {}
    ~MultiDecoder() {
        for (auto& c : chans_) stop_channel(c.get());
    }
    MultiDecoder(const MultiDecoder&) = delete;
    MultiDecoder& operator=(const MultiDecoder&) = delete;

    void set_read_timeout(int seconds) { read_timeout_ms_ = std::max(1, seconds) * 1000; }

    int add_channel(const std::string& url, int /*skip*/, int& loop) {
        std::lock_guard<std::mutex> lk(mu_);
        if (static_cast<int>(chans_.size()) >= max_) return -1;
        auto ch = std::make_unique<Channel>();
        ch->url = url;
        ch->loop = loop;
        ch->thread = std::thread([this, p = ch.get()] { run_channel(p); });
        chans_.push_back(std::move(ch));
        return static_cast<int>(chans_.size()) - 1;
    }
    void del_channel(int ch) {
        std::lock_guard<std::mutex> lk(mu_);
        if (ch < 0 || ch >= static_cast<int>(chans_.size())) return;
        stop_channel(chans_[ch].get());
    }
    std::vector<int> get_frame_shape(int ch) {
        std::lock_guard<std::mutex> lk(mu_);
        if (ch < 0 || ch >= static_cast<int>(chans_.size())) return {};
        auto& c = *chans_[ch];
        std::lock_guard<std::mutex> cl(c.mu);
        return {1, 3, c.height, c.width};
    }
    int read(int ch, BMImage& out, int block) {
        std::unique_lock<std::mutex> lk(mu_);
        if (ch < 0 || ch >= static_cast<int>(chans_.size())) return -1;
        Channel* c = chans_[ch].get();
        lk.unlock();
        std::unique_lock<std::mutex> cl(c->mu);
        uint64_t start = seq_of(c);
        if (block) {
            c->cv.wait_for(cl, std::chrono::milliseconds(read_timeout_ms_),
                           [&] { return c->seq != start && !c->stop; });
        }
        if (c->width <= 0 || !c->has_frame) return -1;
        if (!out.is_created() || out.width() != c->width || out.height() != c->height) {
            out.create(Bmcv::static_handle(), c->height, c->width, FORMAT_BGR_PLANAR,
                       DATA_TYPE_EXT_1N_BYTE);
            out.allocate();
        }
        Bmcv bm(Bmcv::static_handle());
        return bm.image_copy_to(c->frame, out, 0, 0) == 0 ? 0 : -1;
    }
    void reconnect(int ch) {
        std::lock_guard<std::mutex> lk(mu_);
        if (ch < 0 || ch >= static_cast<int>(chans_.size())) return;
        chans_[ch]->reopen = true;
    }
    void enable_dump(int /*ch*/, int /*seconds*/) {}
    int dump(int /*ch*/, int /*pre*/, int /*post*/, std::string& /*path*/) { return -1; }

private:
    struct Channel {
        std::string url;
        int loop = -1;
        std::thread thread;
        std::mutex mu;
        std::condition_variable cv;
        BMImage frame;
        int width = 0, height = 0;
        uint64_t seq = 0;
        bool has_frame = false;
        bool stop = false;
        bool reopen = false;
    };
    static uint64_t seq_of(Channel* c) { return c->seq; }

    void stop_channel(Channel* c) {
        {
            std::lock_guard<std::mutex> cl(c->mu);
            c->stop = true;
        }
        c->cv.notify_all();
        if (c->thread.joinable()) c->thread.join();
    }
    void run_channel(Channel* c) {
        Handle handle(device_id_);
        while (true) {
            {
                std::lock_guard<std::mutex> cl(c->mu);
                if (c->stop) return;
            }
            auto dec = std::make_unique<Decoder>(c->url, true, tpu_id_);
            if (!dec->is_opened()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            while (true) {
                {
                    std::lock_guard<std::mutex> cl(c->mu);
                    if (c->stop) return;
                    if (c->reopen) {
                        c->reopen = false;
                        break;
                    }
                }
                BMImage img;
                int rc = dec->read(handle, img);
                if (rc != 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    break;
                }
                {
                    std::lock_guard<std::mutex> cl(c->mu);
                    c->frame = std::move(img);
                    c->width = c->frame.width();
                    c->height = c->frame.height();
                    c->has_frame = true;
                    ++c->seq;
                }
                c->cv.notify_all();
            }
            if (c->loop >= 0) std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    std::mutex mu_;
    int max_ = 10;
    int device_id_ = 0;
    int tpu_id_ = 0;
    int read_timeout_ms_ = 2000;
    std::vector<std::unique_ptr<Channel>> chans_;
};

// ---------------------------------------------------------------- telemetry
inline int get_dev_id(int dev_id) { return dev_id; }

inline int get_tpu_util(int dev_id) {
    (void)dev_id;
    // /proc/jpuinfo is often not readable inside the container; telemetry only.
    FILE* f = fopen("/proc/jpuinfo", "r");
    if (!f) return 0;
    char line[256];
    int util = 0;
    while (fgets(line, sizeof(line), f)) {
        int v = 0;
        if (sscanf(line, "usage: %d", &v) == 1) util = v;
    }
    fclose(f);
    return util;
}
inline std::vector<int> get_vpu_util(int dev_id) {
    (void)dev_id;
    return {get_tpu_util(dev_id)};
}
inline std::vector<int> get_vpp_util(int dev_id) {
    (void)dev_id;
    return {0};
}
inline std::vector<size_t> get_dev_stat(int dev_id) {
    (void)dev_id;
    return {0, 0};
}
inline int get_chip_temp(int dev_id) {
    (void)dev_id;
    return 0;
}
inline int get_board_temp(int dev_id) {
    (void)dev_id;
    return 0;
}

}  // namespace sail
