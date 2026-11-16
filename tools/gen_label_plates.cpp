// Pre-render every label plate the detector can emit into PNG files plus a
// manifest, so the runtime does not have to render text at task start.
//
// Production ships its label plates together with the algorithm assets; we do
// the same: docker/labels/labels.json lists {text,file} pairs and the task only
// loads PNGs (fast path copies them on-device, no FreeType per frame).
//
// Build (on the box, after `source env/env.sh`):
//   cmake --build build/aidetect -j8 --target gen_label_plates
// Run:
//   ./build/aidetect/gen_label_plates docker/labels
#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using nlohmann::json;

static std::string slug(int i) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "plate_%02d", i);
    return buf;
}

int main(int argc, char** argv) {
    const std::string out_dir = argc > 1 ? argv[1] : "docker/labels";
    const std::string font_path = argc > 2 ? argv[2]
                                           : "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc";
    // Every label string the detector can produce (model.hpp report_content)
    // plus the behaviour combinations built from the enabled switches.
    std::vector<std::string> texts = {
        "未戴安全帽", "未穿反光衣", "未穿救生衣", "未穿安全帽", "抽烟", "人员",
        "抽烟行为", "抽烟打电话", "抽烟/打电话", "人员聚集", "人数监控",
        "抽烟", "打电话", "玩手机",
        "抽烟/打电话", "抽烟/玩手机", "打电话/玩手机", "抽烟/打电话/玩手机",
    };
    std::sort(texts.begin(), texts.end());
    texts.erase(std::unique(texts.begin(), texts.end()), texts.end());

    cv::Ptr<cv::freetype::FreeType2> ft2;
    try {
        ft2 = cv::freetype::createFreeType2();
        ft2->loadFontData(font_path, 0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "font load failed: %s\n", e.what());
        return 2;
    }

    const int font = 30;
    json manifest = json::array();
    int idx = 0;
    for (const auto& text : texts) {
        // Same recipe as the runtime fallback: draw on a generous red canvas,
        // then crop to the real glyph bounding box (getTextSize under-reports
        // the rendered advance and clipped the last character).
        cv::Mat canvas(96, 640, CV_8UC3, cv::Scalar(0, 0, 255));
        try {
            ft2->putText(canvas, text, cv::Point(12, 8), font,
                         cv::Scalar(255, 255, 255), 1, cv::LINE_AA, false);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "putText failed for %s: %s\n", text.c_str(), e.what());
            continue;
        }
        cv::Mat mask;
        cv::inRange(canvas, cv::Scalar(200, 200, 200), cv::Scalar(255, 255, 255), mask);
        cv::Rect bb = cv::boundingRect(mask);
        if (bb.width <= 0 || bb.height <= 0) {
            std::fprintf(stderr, "empty glyph box for %s\n", text.c_str());
            continue;
        }
        const int pad_x = 8, pad_y = 4;
        int x0 = std::max(0, bb.x - pad_x);
        int y0 = std::max(0, bb.y - pad_y);
        int x1 = std::min(canvas.cols, bb.x + bb.width + pad_x);
        int y1 = std::min(canvas.rows, bb.y + bb.height + pad_y);
        cv::Mat plate = canvas(cv::Rect(x0, y0, x1 - x0, y1 - y0)).clone();
        const std::string file = slug(idx) + ".png";
        if (!cv::imwrite(out_dir + "/" + file, plate)) {
            std::fprintf(stderr, "imwrite failed: %s\n", file.c_str());
            return 3;
        }
        manifest.push_back({{"text", text}, {"file", file},
                            {"w", plate.cols}, {"h", plate.rows}});
        std::printf("%s -> %s (%dx%d)\n", text.c_str(), file.c_str(), plate.cols,
                    plate.rows);
        ++idx;
    }
    std::ofstream f(out_dir + "/labels.json", std::ios::binary);
    f << manifest.dump(2) << "\n";
    std::printf("wrote %d plates to %s\n", idx, out_dir.c_str());
    return 0;
}
