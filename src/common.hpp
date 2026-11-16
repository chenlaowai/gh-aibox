#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <nlohmann/json.hpp>

namespace aidetect {

using Json = nlohmann::json;
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// ---------------------------------------------------------------------------
// logging
// ---------------------------------------------------------------------------
inline std::mutex& log_mutex() {
    static std::mutex m;
    return m;
}

// JPU is a single shared encoder/decoder engine. Concurrent JPEG encode/decode
// from many task threads can wedge the driver; serialize those calls.
inline std::mutex& jpeg_hw_mutex() {
    static std::mutex m;
    return m;
}

// ISC previewURL requests are serialized to avoid a 30-task startup burst
// overwhelming the identity server.
inline std::mutex& isc_resolve_mutex() {
    static std::mutex m;
    return m;
}

// FreeType2 objects are not safe to call concurrently. Status heartbeat and
// detection rendering share one Task-local FreeType object.
inline std::mutex& font_mutex() {
    static std::mutex m;
    return m;
}

// MultiDecoder channel creation/destruction touches the shared JPU/VPU driver.
// Serialize it; concurrent add_channel from 36 task threads crashed Jpu_GetError.
inline std::mutex& decode_hw_mutex() {
    static std::mutex m;
    return m;
}



// VPU/JPU capacity guard for high-resolution decoders. 36 simultaneous
// 1080p decoders can wedge the driver; keep a conservative admission limit.
class HighResGate {
public:
    bool acquire() {
        std::lock_guard<std::mutex> lk(m_);
        if (active_ >= max_) return false;
        ++active_;
        return true;
    }
    void release() {
        std::lock_guard<std::mutex> lk(m_);
        if (active_ > 0) --active_;
    }
    int active() {
        std::lock_guard<std::mutex> lk(m_);
        return active_;
    }
    int max() const { return max_; }

private:
    std::mutex m_;
    int active_ = 0;
    int max_ = [] {
        const char* e = std::getenv("AIDETECT_MAX_HIGHRES_TASKS");
        return e ? std::max(1, std::atoi(e)) : 12;
    }();
};

inline HighResGate& high_res_gate() {
    static HighResGate g;
    return g;
}

struct HighResLease {
    bool held = false;
    ~HighResLease() {
        if (held) high_res_gate().release();
    }
};

inline void log_line(const char* level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(log_mutex());
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    std::cerr << buf << " [" << level << "] " << msg << std::endl;
}

inline void log_info(const std::string& msg) { log_line("INFO", msg); }
inline void log_warn(const std::string& msg) { log_line("WARN", msg); }
inline void log_error(const std::string& msg) { log_line("ERROR", msg); }

// ---------------------------------------------------------------------------
// base64url / JWT
// ---------------------------------------------------------------------------
inline std::string base64_encode(const unsigned char* data, size_t len) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    size_t i = 0;
    while (i + 2 < len) {
        uint32_t v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(tbl[(v >> 18) & 63]);
        out.push_back(tbl[(v >> 12) & 63]);
        out.push_back(tbl[(v >> 6) & 63]);
        out.push_back(tbl[v & 63]);
        i += 3;
    }
    if (i < len) {
        uint32_t v = data[i] << 16;
        bool two = (i + 1 < len);
        if (two) v |= data[i + 1] << 8;
        out.push_back(tbl[(v >> 18) & 63]);
        out.push_back(tbl[(v >> 12) & 63]);
        out.push_back(two ? tbl[(v >> 6) & 63] : '=');
        out.push_back('=');
    }
    return out;
}

inline std::string base64url_encode(const std::string& s) {
    std::string out = base64_encode(
        reinterpret_cast<const unsigned char*>(s.data()), s.size());
    for (char& c : out) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!out.empty() && out.back() == '=') out.pop_back();
    return out;
}

inline int b64val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-' || c == '+') return 62;
    if (c == '_' || c == '/') return 63;
    return -1;
}

inline std::optional<std::string> base64url_decode(const std::string& in) {
    std::string s;
    s.reserve(in.size());
    for (char c : in) {
        if (c == '=') break;
        s.push_back(c);
    }
    if (s.size() % 4 == 1) return std::nullopt;
    while (s.size() % 4) s.push_back('=');
    std::string out;
    out.reserve(s.size() / 4 * 3);
    for (size_t i = 0; i < s.size(); i += 4) {
        int a = b64val(s[i]);
        int b = b64val(s[i + 1]);
        int c = (s[i + 2] == '=') ? 0 : b64val(s[i + 2]);
        int d = (s[i + 3] == '=') ? 0 : b64val(s[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return std::nullopt;
        uint32_t v = (a << 18) | (b << 12) | (c << 6) | d;
        out.push_back(static_cast<char>((v >> 16) & 0xff));
        if (s[i + 2] != '=') out.push_back(static_cast<char>((v >> 8) & 0xff));
        if (s[i + 3] != '=') out.push_back(static_cast<char>(v & 0xff));
    }
    return out;
}

inline std::string hmac_sha256(const std::string& key, const std::string& data) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &len);
    return std::string(reinterpret_cast<char*>(digest), len);
}

struct JwtPayload {
    Json json;
    std::string user_id;
};

class Jwt {
public:
    explicit Jwt(std::string secret) : secret_(std::move(secret)) {}

    std::string create(const std::string& user_id = "admin") const {
        Json header = {{"alg", "HS256"}, {"typ", "JWT"}};
        auto now = static_cast<long long>(std::time(nullptr));
        Json payload = {{"user_id", user_id},
                        {"iss", "HNKC.AIBOX"},
                        {"aud", "HNKC.AIBOX"},
                        {"exp", now + 7 * 24 * 3600},
                        {"iat", now}};
        std::string h = base64url_encode(header.dump());
        std::string p = base64url_encode(payload.dump());
        std::string signing = h + "." + p;
        std::string sig = hmac_sha256(secret_, signing);
        return signing + "." + base64url_encode(sig);
    }

    std::optional<JwtPayload> verify(const std::string& token) const {
        if (secret_.empty()) return std::nullopt;
        size_t p1 = token.find('.');
        if (p1 == std::string::npos) return std::nullopt;
        size_t p2 = token.find('.', p1 + 1);
        if (p2 == std::string::npos) return std::nullopt;
        std::string h = token.substr(0, p1);
        std::string p = token.substr(p1 + 1, p2 - p1 - 1);
        std::string s = token.substr(p2 + 1);
        std::string signing = h + "." + p;
        std::string expect = base64url_encode(hmac_sha256(secret_, signing));
        if (s.size() != expect.size() ||
            CRYPTO_memcmp(s.data(), expect.data(), s.size()) != 0) {
            return std::nullopt;
        }
        auto payload_raw = base64url_decode(p);
        auto header_raw = base64url_decode(h);
        if (!payload_raw || !header_raw) return std::nullopt;
        Json header, payload;
        try {
            header = Json::parse(*header_raw);
            payload = Json::parse(*payload_raw);
        } catch (...) {
            return std::nullopt;
        }
        if (header.value("alg", "") != "HS256") return std::nullopt;
        if (payload.value("iss", "") != "HNKC.AIBOX") return std::nullopt;
        if (payload.value("aud", "") != "HNKC.AIBOX") return std::nullopt;
        auto now = static_cast<long long>(std::time(nullptr));
        if (payload.contains("exp") && payload["exp"].is_number() &&
            payload["exp"].get<long long>() < now) {
            return std::nullopt;
        }
        JwtPayload out;
        out.json = payload;
        if (payload.contains("user_id") && payload["user_id"].is_string()) {
            out.user_id = payload["user_id"].get<std::string>();
        } else if (payload.contains(
                       "http://schemas.xmlsoap.org/ws/2005/05/identity/"
                       "claims/name") &&
                   payload["http://schemas.xmlsoap.org/ws/2005/05/identity/"
                           "claims/name"]
                       .is_string()) {
            out.user_id =
                payload["http://schemas.xmlsoap.org/ws/2005/05/identity/claims/"
                        "name"]
                    .get<std::string>();
        }
        return out;
    }

private:
    std::string secret_;
};

// ---------------------------------------------------------------------------
// small HTTP client (http only; backend endpoints are http)
// ---------------------------------------------------------------------------
struct HttpResponse {
    int status = 0;
    std::string body;
};

struct ParsedUrl {
    std::string host;
    std::string port = "80";
    std::string target = "/";
};

inline std::optional<ParsedUrl> parse_http_url(const std::string& url) {
    std::string rest;
    if (url.rfind("http://", 0) == 0) rest = url.substr(7);
    else if (url.rfind("https://", 0) == 0) return std::nullopt;  // no TLS client
    else rest = url;
    size_t slash = rest.find('/');
    std::string authority = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    ParsedUrl out;
    out.target = (slash == std::string::npos) ? "/" : rest.substr(slash);
    size_t colon = authority.find(':');
    if (colon == std::string::npos) {
        out.host = authority;
    } else {
        out.host = authority.substr(0, colon);
        out.port = authority.substr(colon + 1);
    }
    if (out.host.empty()) return std::nullopt;
    return out;
}

inline std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

class HttpClient {
public:
    static HttpResponse request(const std::string& method, const std::string& url,
                                const std::string& body = "",
                                const std::map<std::string, std::string>& headers = {},
                                int timeout_ms = 5000) {
        HttpResponse result;
        auto parsed = parse_http_url(url);
        if (!parsed) {
            result.status = -1;
            result.body = "unsupported url";
            return result;
        }
        try {
            asio::io_context ioc;
            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);
            auto const results = resolver.resolve(parsed->host, parsed->port);
            stream.expires_after(std::chrono::milliseconds(timeout_ms));
            stream.connect(results);
            http::request<http::string_body> req{
                (method == "POST") ? http::verb::post : http::verb::get,
                parsed->target, 11};
            req.set(http::field::host, parsed->host);
            req.set(http::field::user_agent, "aidetect/0.1");
            req.set(http::field::accept, "application/json");
            for (auto& kv : headers) req.set(kv.first, kv.second);
            if (!body.empty()) {
                req.set(http::field::content_type, "application/json");
                req.body() = body;
                req.prepare_payload();
            }
            http::write(stream, req);
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);
            result.status = res.result_int();
            result.body = res.body();
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        } catch (const std::exception& e) {
            result.status = -1;
            result.body = e.what();
        }
        return result;
    }

    static HttpResponse get(const std::string& url,
                            const std::map<std::string, std::string>& headers = {},
                            int timeout_ms = 5000) {
        return request("GET", url, "", headers, timeout_ms);
    }
    static HttpResponse post(const std::string& url, const std::string& body,
                             const std::map<std::string, std::string>& headers = {},
                             int timeout_ms = 5000) {
        return request("POST", url, body, headers, timeout_ms);
    }
};

// ---------------------------------------------------------------------------
// config
// ---------------------------------------------------------------------------
struct ModelConfig {
    std::string name;
    float conf = 0.25f;
    float nms = 0.7f;
    int exceed = 1;
    float attr_thresh = 0.0f;
    // Production per-algorithm parameters (UserData / tb_algo_config).  These
    // are exposed by the vendor Web UI and differ per algorithm.
    float conf_no_helmet = 0.0f;   // helmet_v3 no_helmet_det_threshold
    float keep_sec = 0.0f;        // violation must persist this long before alarm
    float repeat_sec = 0.0f;      // minimum repeat interval for this algorithm
    bool has_repeat_param = false; // explicit 0 = report once per target
    float sensitivity = 0.0f;     // report sensitivity (vest/sleeve)
    float calling_keep_sec = 0.0f;
    float playing_keep_sec = 0.0f;
    float smoking_keep_sec = 1.0f;  // production smoking is short; call/play use 10s
    float smoking_thresh = 0.5f;
    float calling_thresh = 0.5f;
    float phone_thresh = 0.5f;
    float cluster_expand = 0.0f;  // people_gather cluster_width_expand_ratio
    int cluster_people = 0;       // people_gather cluster_people_threshold
    bool behavior_enable_smoking = true;
    bool behavior_enable_calling = true;
    bool behavior_enable_playing = true;
    // Production task-level "人员检测基础配置" (UserData).
    float people_conf = 0.3f;      // threshold_for_people
    bool people_deep = false;      // enable_people_deep_mode
    std::vector<std::vector<std::pair<float, float>>> polygons;  // normalized 0..1
};

struct TaskConfig {
    std::string task_id;
    std::string task_name;        // production AlgoTaskSession / task code shown on merge tiles
    std::string channel_address;
    int frame_interval = 5;
    int detect_threshold = 6;
    int detect_section = 10;
    int video_duration = 10;
    int cooldown_seconds = 180;
    std::vector<ModelConfig> models;

    const ModelConfig* find_model(const std::string& name) const {
        for (auto& m : models) if (m.name == name) return &m;
        return nullptr;
    }
};

struct AppConfig {
    int port = 9000;
    std::string jwt_secret;
    std::string config_base = "http://10.10.206.68";
    std::string report_base = "http://127.0.0.1";
    std::string model_root = "/data/CT/project/ai/models/BM1684X";
    std::string data_root = "/data/CT/project/ai/inference/save";
    std::string tmp_root = "/data/CT/project/ai/inference/video_transtation";
    std::string config_file;  // optional local mock
    std::string font_path = "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc";
    std::string reflective_model;

    static AppConfig from_env() {
        AppConfig c;
        auto env = [](const char* k) -> std::string {
            const char* v = std::getenv(k);
            return v ? v : "";
        };
        auto env_int = [&](const char* k, int def) {
            auto s = env(k);
            return s.empty() ? def : std::atoi(s.c_str());
        };
        c.port = env_int("AIDETECT_PORT", 9000);
        c.jwt_secret = env("AIDETECT_JWT_SECRET");
        if (!env("AIDETECT_CONFIG_BASE").empty()) c.config_base = env("AIDETECT_CONFIG_BASE");
        if (!env("AIDETECT_REPORT_BASE").empty()) c.report_base = env("AIDETECT_REPORT_BASE");
        if (!env("AIDETECT_MODEL_ROOT").empty()) c.model_root = env("AIDETECT_MODEL_ROOT");
        if (!env("AIDETECT_DATA_ROOT").empty()) c.data_root = env("AIDETECT_DATA_ROOT");
        if (!env("AIDETECT_TMP_ROOT").empty()) c.tmp_root = env("AIDETECT_TMP_ROOT");
        if (!env("AIDETECT_FONT").empty()) c.font_path = env("AIDETECT_FONT");
        c.config_file = env("AIDETECT_CONFIG_FILE");
        c.reflective_model = c.model_root + "/yolov11s_fp16_1b_back_reflectivejacket_1.bmodel";
        return c;
    }
};

inline int parse_int(const Json& j, const char* key, int def) {
    if (!j.contains(key)) return def;
    try {
        if (j[key].is_number()) return j[key].get<int>();
        if (j[key].is_string()) return std::atoi(j[key].get<std::string>().c_str());
    } catch (...) {}
    return def;
}

inline float parse_float(const Json& j, const char* key, float def) {
    if (!j.contains(key)) return def;
    try {
        if (j[key].is_number()) return j[key].get<float>();
        if (j[key].is_string()) return std::atof(j[key].get<std::string>().c_str());
    } catch (...) {}
    return def;
}

inline bool parse_bool(const Json& j, const char* key, bool def) {
    if (!j.contains(key)) return def;
    try {
        if (j[key].is_boolean()) return j[key].get<bool>();
        if (j[key].is_number()) return j[key].get<int>() != 0;
        if (j[key].is_string()) {
            std::string s = j[key].get<std::string>();
            return s == "true" || s == "True" || s == "1" || s == "1.0";
        }
    } catch (...) {}
    return def;
}

inline TaskConfig parse_task_config(const Json& root) {
    Json data = root;
    if (root.contains("data") && root["data"].is_object()) data = root["data"];
    TaskConfig cfg;
    cfg.task_id = data.value("taskId", "");
    // Production's OSD line on every merge tile is the task session/code
    // (AlgTaskSession).  TaskAlgoConfigDetail exposes it as taskName.
    cfg.task_name = data.value("taskName", "");
    if (cfg.task_name.empty()) cfg.task_name = data.value("taskCode", "");
    cfg.channel_address = data.value("channelAddress", "");
    cfg.frame_interval = parse_int(data, "frameInterval", 5);
    cfg.detect_threshold = parse_int(data, "detectThreshold", 6);
    cfg.detect_section = parse_int(data, "detectSection", 10);
    cfg.video_duration = parse_int(data, "videoDuration", 10);
    cfg.cooldown_seconds = parse_int(data, "cooldownTime", 3) * 60;
    if (data.contains("model") && data["model"].is_array()) {
        for (auto& m : data["model"]) {
            ModelConfig mc;
            mc.name = m.value("algoCode", "");
            if (mc.name.empty()) continue;
            mc.exceed = parse_int(m, "exceed_num", 1);
            if (m.contains("algoConfig") && m["algoConfig"].is_array()) {
                for (auto& c : m["algoConfig"]) {
                    auto code = c.value("code", "");
                    if (code == "conf_thresh") mc.conf = parse_float(c, "configValue", 0.25f);
                    if (code == "nms_thresh") mc.nms = parse_float(c, "configValue", 0.7f);
                    if (code == "exceed_num") mc.exceed = parse_int(c, "configValue", 1);
                    if (code == "attr_thresh") mc.attr_thresh = parse_float(c, "configValue", 0.0f);
                    // Production exposes per-algorithm thresholds. Map the
                    // classifier/people thresholds onto our internal fields.
                    if (code == "no_helmet_det_threshold" || code == "vest_thresh" ||
                        code == "smoking_thresh" || code == "calling_thresh" ||
                        code == "phone_thresh" || code == "lifejacket_thresh" ||
                        code == "helmet_3cls_no_helmet_threshold" ||
                        code == "helmet_3cls_no_vest_threshold" ||
                        code == "helmet_3cls_no_sleeve_threshold" ||
                        code == "safety_jacket_threshold" ||
                        code == "threshold_ppe_alarm_sensitive")
                        mc.attr_thresh = parse_float(c, "configValue", mc.attr_thresh);
                    if (code == "people_ciga_threshold" ||
                        code == "people_call_threshold" ||
                        code == "people_phone_threshold" ||
                        code == "people_hand_threshold" ||
                        code == "people_mouth_threshold") {
                        float t = parse_float(c, "configValue", mc.attr_thresh);
                        mc.attr_thresh = std::max(mc.attr_thresh, t);
                        if (code == "people_ciga_threshold") mc.smoking_thresh = t;
                        if (code == "people_call_threshold") mc.calling_thresh = t;
                        if (code == "people_phone_threshold") mc.phone_thresh = t;
                    }
                    if (code == "people_thresh" || code == "crowd_thresh" ||
                        code == "head_thresh" || code == "helmet_det_threshold" ||
                        code == "threshold_for_head" ||
                        code == "threshold_for_face" || code == "crowd_checked_threshold")
                        mc.conf = parse_float(c, "configValue", mc.conf);
                    if (code == "threshold_for_people")
                        mc.people_conf = parse_float(c, "configValue", mc.people_conf);
                    if (code == "enable_people_deep_mode")
                        mc.people_deep = parse_bool(c, "configValue", mc.people_deep);
                    if (code == "no_helmet_det_threshold")
                        mc.conf_no_helmet = parse_float(c, "configValue", 0.7f);
                    if (code == "helmet_v3_keep_no_sec" ||
                        code == "helmet_3cls_no_helmet_sec" ||
                        code == "helmet_3cls_no_vest_sec" ||
                        code == "helmet_3cls_no_sleeve_sec" ||
                        code == "safety_jacket_duration_sec" ||
                        code == "threshold_ppe_alarm_duration_sec" ||
                        code == "no_suit_keep_sec")
                        mc.keep_sec = parse_float(c, "configValue", mc.keep_sec);
                    // Production names this parameter differently per
                    // algorithm (helmet_v3_repeat_sec, safety_jacket_alarm_sec,
                    // helmet_3cls_*_alarm_sec, ...).  Accept the explicit list
                    // and any future *repeat* / *alarm_sec* variant so a
                    // 86400s "最小上报间隔" can never silently fall back to the
                    // 3-minute task cooldown.
                    const bool repeat_code =
                        code.find("repeat") != std::string::npos ||
                        code.find("alarm_sec") != std::string::npos ||
                        code.find("report_interval") != std::string::npos;
                    if (repeat_code ||
                        code == "helmet_v3_repeat_sec" ||
                        code == "helmet_3cls_no_helmet_alarm_sec" ||
                        code == "helmet_3cls_no_vest_alarm_sec" ||
                        code == "helmet_3cls_no_sleeve_alarm_sec" ||
                        code == "safety_jacket_alarm_sec" ||
                        code == "no_suit_interval_sec") {
                        mc.repeat_sec = parse_float(c, "configValue", mc.repeat_sec);
                        mc.has_repeat_param = true;
                    }
                    if (code == "sensitivity_vest_cls" ||
                        code == "sensitivity_sleeve_cls" ||
                        code == "sensitivity_helmet_v3")
                        mc.sensitivity = parse_float(c, "configValue", mc.sensitivity);
                    if (code == "calling_max_sec") {
                        mc.calling_keep_sec = parse_float(c, "configValue", mc.calling_keep_sec);
                        mc.keep_sec = std::max(mc.keep_sec, mc.calling_keep_sec);
                    }
                    if (code == "playing_max_sec") {
                        mc.playing_keep_sec = parse_float(c, "configValue", mc.playing_keep_sec);
                        mc.keep_sec = std::max(mc.keep_sec, mc.playing_keep_sec);
                    }
                    if (code == "cluster_width_expand_ratio")
                        mc.cluster_expand = parse_float(c, "configValue", mc.cluster_expand);
                    if (code == "cluster_people_threshold")
                        mc.cluster_people = parse_int(c, "configValue", mc.cluster_people);
                    if (code == "behavior_enable_smoking")
                        mc.behavior_enable_smoking = parse_bool(c, "configValue", mc.behavior_enable_smoking);
                    if (code == "behavior_enable_calling")
                        mc.behavior_enable_calling = parse_bool(c, "configValue", mc.behavior_enable_calling);
                    if (code == "behavior_enable_playing")
                        mc.behavior_enable_playing = parse_bool(c, "configValue", mc.behavior_enable_playing);
                }
            }
            if (m.contains("taskArea") && m["taskArea"].is_array()) {
                for (auto& area : m["taskArea"]) {
                    std::string pts = area.value("points", "");
                    if (pts.empty()) continue;
                    try {
                        auto arr = Json::parse(pts);
                        std::vector<std::pair<float, float>> poly;
                        for (auto& p : arr) {
                            float x = p.value("X", 0.f);
                            float y = p.value("Y", 0.f);
                            poly.emplace_back(x, y);
                        }
                        if (poly.size() >= 3) mc.polygons.push_back(std::move(poly));
                    } catch (...) {}
                }
            }
            cfg.models.push_back(std::move(mc));
        }
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// files / strings
// ---------------------------------------------------------------------------
inline bool ensure_dir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec;
}

inline std::string now_stamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmv);
    return buf;
}

inline std::string stamp_at_ms(int64_t ms) {
    std::time_t t = static_cast<std::time_t>(ms / 1000);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmv);
    return buf;
}

inline std::string now_readable() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

inline std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline bool write_file(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

// A JPEG that the JPU/decoder can turn into garbage (or that was written while
// the frame was being recycled) still "looks like" a JPEG unless the SOI/EOI
// markers are checked.  The merge path uses this to drop a bad tile instead of
// compositing a colour-noise tile (KNOWN-ISSUES KI-030).
inline bool jpeg_markers_ok(const std::vector<unsigned char>& buf) {
    if (buf.size() < 4) return false;
    if (buf[0] != 0xFF || buf[1] != 0xD8) return false;
    const size_t n = buf.size();
    // EOI must be at the end, or within the last few bytes if the encoder pads.
    for (size_t i = 0; i < 8 && i + 2 <= n; ++i) {
        const size_t p = n - 2 - i;
        if (buf[p] == 0xFF && buf[p + 1] == 0xD9) return true;
    }
    return false;
}

inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// AIDETECT_PROFILE (env) or the presence of /tmp/aidetect_profile enables the
// periodic inference/preview counters.  The file check is cached for one
// second so it can be toggled on a running container without a restart.
inline bool profiling() {
    static const bool env = std::getenv("AIDETECT_PROFILE") != nullptr;
    if (env) return true;
    static std::atomic<int64_t> last{0};
    static std::atomic<bool> on{false};
    int64_t t = now_ms();
    int64_t prev = last.load();
    if (t - prev >= 1000 && last.compare_exchange_strong(prev, t)) {
        std::ifstream f("/tmp/aidetect_profile");
        on.store(f.good());
    }
    return on.load();
}

// ---------------------------------------------------------------------------
// metrics (requires sail)
// ---------------------------------------------------------------------------
struct CpuSample {
    unsigned long long total = 0;
    unsigned long long idle = 0;
};

inline CpuSample read_cpu_sample() {
    CpuSample s;
    std::ifstream f("/proc/stat");
    std::string cpu;
    unsigned long long v[10] = {0};
    if (f >> cpu >> v[0] >> v[1] >> v[2] >> v[3] >> v[4] >> v[5] >> v[6] >> v[7]) {
        for (int i = 0; i < 8; ++i) s.total += v[i];
        s.idle = v[3] + v[4];
    }
    return s;
}

inline int read_mem_percent() {
    std::ifstream f("/proc/meminfo");
    std::string key;
    long long value = 0;
    long long total = 0, avail = 0;
    std::string unit;
    while (f >> key >> value >> unit) {
        if (key == "MemTotal:") total = value;
        if (key == "MemAvailable:") avail = value;
    }
    if (total <= 0) return 0;
    return static_cast<int>((total - avail) * 100 / total);
}

inline double disk_free_gb(const std::string& path) {
    struct statvfs st {};
    if (statvfs(path.c_str(), &st) != 0) return 0.0;
    return static_cast<double>(st.f_bavail) * st.f_frsize / 1024.0 / 1024.0 / 1024.0;
}

inline std::string http_post_raw(const std::string& host, const std::string& port,
                                 const std::string& target, const std::string& body,
                                 int timeout_ms = 15000) {
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;
    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        auto const results = resolver.resolve(host, port);
        stream.expires_after(std::chrono::milliseconds(timeout_ms));
        stream.connect(results);
        http::request<http::string_body> req{http::verb::post, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::content_type, "application/json");
        req.body() = body;
        req.prepare_payload();
        http::write(stream, req);
        beast::flat_buffer buffer;
        http::response<http::vector_body<unsigned char>> res;
        http::read(stream, buffer, res);
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        return std::string(reinterpret_cast<const char*>(res.body().data()),
                           res.body().size());
    } catch (...) {
        return {};
    }
}

}  // namespace aidetect
