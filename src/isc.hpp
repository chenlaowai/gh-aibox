#pragma once

#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>

#include "common.hpp"

namespace aidetect {

struct IscUrl {
    std::string app_key;
    std::string app_secret;
    std::string host;
    std::string port;
    std::string camera_index;
    int stream_type = 0;
    // Production MediaUrl: ...&protocol=rtmp&https=1&videotype=h264&transcode=1
    // The vendor forwards these to Artemis; we used to hardcode protocol=rtsp.
    std::string protocol = "rtsp";
    int transcode = -1;
    std::string videotype;
    int https = -1;
};

class IscResolver {
public:
    static int stream_type_of(const std::string& isc_url) {
        try { return parse(isc_url).stream_type; } catch (...) { return 0; }
    }

    static std::string resolve(const std::string& isc_url) {
        return resolve(isc_url, "", -1);
    }

    static std::string resolve(const std::string& isc_url,
                               const std::string& protocol_override,
                               int stream_type_override) {
        // ISC preview calls are expensive and the ISC server dislikes a burst
        // of simultaneous previewURL requests. Serialize them so all task
        // starts do not race for one camera/session.
        std::lock_guard<std::mutex> isc_lock(isc_resolve_mutex());
        IscUrl u = parse(isc_url);
        std::string nonce = random_uuid();
        long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
        std::string ts = std::to_string(now_ms);
        const std::string api = "/artemis/api/video/v2/cameras/previewURLs";
        std::string sign_str = "POST\n*/*\napplication/json" +
                               std::string("\nx-ca-key:") + u.app_key +
                               "\nx-ca-nonce:" + nonce +
                               "\nx-ca-timestamp:" + ts + "\n" + api;
        std::string digest = hmac_sha256(u.app_secret, sign_str);
        std::string sig = base64_encode(
            reinterpret_cast<const unsigned char*>(digest.data()), digest.size());
        if (stream_type_override >= 0) u.stream_type = stream_type_override;
        std::string proto = u.protocol;
        if (!proto.empty() && proto != "rtsp" && proto != "rtmp")
            proto = "rtsp";
        // AIDETECT_ISC_PROTOCOL is an explicit per-box override: on this box
        // the ISC RTMP gateway (1935) is reachable but its per-camera transcode
        // session pool is tiny, so RTMP attempts stall for 15-20s and starve
        // the reconnect loop.  When the operator pins the protocol, it must win
        // over the candidate protocol chosen by the task.
        const char* force_proto = std::getenv("AIDETECT_ISC_PROTOCOL");
        if (force_proto && *force_proto)
            proto = force_proto;
        else if (!protocol_override.empty())
            proto = protocol_override;
        if (proto != "rtsp" && proto != "rtmp") proto = "rtsp";
        const char* rtmp_proxy = std::getenv("AIDETECT_ISC_RTMP_PROXY");
        const bool have_rtmp_proxy = rtmp_proxy && *rtmp_proxy;
        const char* allow_direct_rtmp = std::getenv("AIDETECT_ISC_ALLOW_DIRECT_RTMP");
        if (proto == "rtmp" && !have_rtmp_proxy &&
            !(allow_direct_rtmp && *allow_direct_rtmp)) {
            // The backend asks for RTMP (like the vendor), but this box may not
            // have a route/relay to 10.10.69.250:1935. Keep the working RTSP
            // path unless a relay or explicit direct access is configured.
            log_warn("ISC rtmp requested but no AIDETECT_ISC_RTMP_PROXY/"
                     "AIDETECT_ISC_ALLOW_DIRECT_RTMP; using rtsp");
            proto = "rtsp";
        }
        // Reuse the ISC previewURL per camera/protocol/streamType. Every task
        // reconnect used to request a brand-new ISC session; with 30+ tasks
        // that exhausted the ISC session pool and produced DESCRIBE 500s.
        std::string cache_key = isc_url + "|" + proto + "|" +
                                std::to_string(u.stream_type);
        std::string stale_url;
        {
            std::lock_guard<std::mutex> lk(cache_mu_);
            auto it = url_cache_.find(cache_key);
            if (it != url_cache_.end()) {
                if (now_ms - it->second.second < 1800000)
                    return it->second.first;
                // Keep the last URL as a fallback.  ISC sometimes answers
                // "missing data.url" when its previewURL pool is busy; the
                // existing media session may still be alive, and opening the
                // cached URL is better than leaving the task dark until a
                // manual restart.
                stale_url = it->second.first;
            }
        }
        // Mutex serializes previewURL creation, but 30 tasks still fired
        // requests back-to-back and ISC intermittently replied with
        // "missing data.url".  Leave a small gap between cache misses so the
        // ISC previewURL pool is not hammered.
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        Json body = {{"cameraIndexCode", u.camera_index},
                     {"streamType", u.stream_type},
                     {"protocol", proto}};
        if (u.transcode >= 0) body["transcode"] = u.transcode;
        if (!u.videotype.empty()) body["videotype"] = u.videotype;
        if (u.https >= 0) body["https"] = u.https;
        std::map<std::string, std::string> headers{
            {"Accept", "*/*"},
            {"Content-Type", "application/json"},
            {"x-ca-key", u.app_key},
            {"x-ca-signature-headers", "x-ca-key,x-ca-nonce,x-ca-timestamp"},
            {"x-ca-signature", sig},
            {"x-ca-timestamp", ts},
            {"x-ca-nonce", nonce}};
        log_info("ISC resolve host=" + u.host + " port=" + u.port +
                 " camera=" + u.camera_index.substr(0, 8) + "...");
        std::string url;
        try {
            auto resp = https_post(u.host, u.port, api, body.dump(), headers, 10000);
            if (resp.status < 200 || resp.status >= 300) {
                throw std::runtime_error("ISC HTTP " + std::to_string(resp.status));
            }
            auto j = Json::parse(resp.body);
            if (!j.contains("data") || !j["data"].contains("url")) {
                throw std::runtime_error("ISC response missing data.url");
            }
            url = j["data"]["url"].get<std::string>();
            if (url.empty()) throw std::runtime_error("ISC returned empty url");
        } catch (const std::exception& e) {
            if (!stale_url.empty()) {
                log_warn(std::string("ISC resolve failed (") + e.what() +
                         "), reusing cached url for " + u.camera_index.substr(0, 8));
                return stale_url;
            }
            throw;
        }
        log_info("ISC url protocol=" + url.substr(0, url.find(':')));
        if (url.rfind("rtmp://", 0) == 0 && have_rtmp_proxy) {
            std::string rest = url.substr(7);
            size_t slash = rest.find('/');
            std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
            std::string tail = slash == std::string::npos ? "" : rest.substr(slash);
            std::string proxied = "rtmp://" + std::string(rtmp_proxy) + tail;
            log_info("ISC RTMP target " + authority + " via proxy " +
                     std::string(rtmp_proxy));
            return proxied;
        }
        const char* rtsp_proxy = std::getenv("AIDETECT_ISC_RTSP_PROXY");
        if (rtsp_proxy && *rtsp_proxy && url.rfind("rtsp://", 0) == 0) {
            std::string rest = url.substr(7);
            std::string userinfo;
            size_t at = rest.find('@');
            size_t slash = rest.find('/');
            if (at != std::string::npos && (slash == std::string::npos || at < slash)) {
                userinfo = rest.substr(0, at + 1);
                rest = rest.substr(at + 1);
            }
            size_t path = rest.find('/');
            std::string target = path == std::string::npos ? rest : rest.substr(0, path);
            std::string tail = path == std::string::npos ? "" : rest.substr(path);
            std::string proxied = "rtsp://" + userinfo + rtsp_proxy + tail;
            log_info("ISC RTSP target " + target + " via proxy " + rtsp_proxy);
            return proxied;
        }
        {
            std::lock_guard<std::mutex> lk(cache_mu_);
            url_cache_[cache_key] = {url, now_ms};
        }
        return url;
    }

    // A cached previewURL is a live media session, not a permanent address.
    // The vendor re-resolves the previewURL on every reconnect; a box that keeps
    // using a dead URL forever stays "connecting" until the 30 min cache expiry.
    // Call this when an open/read on the URL failed so the next attempt asks
    // ISC for a fresh session (production does exactly this per reconnect).
    static void invalidate(const std::string& isc_url) {
        const std::string prefix = isc_url + "|";
        std::lock_guard<std::mutex> lk(cache_mu_);
        for (auto it = url_cache_.begin(); it != url_cache_.end();) {
            if (it->first.rfind(prefix, 0) == 0)
                it = url_cache_.erase(it);
            else
                ++it;
        }
    }

private:
    // previewURL cache: key = isc_url + "|" + protocol + "|" + streamType.
    inline static std::mutex cache_mu_;
    inline static std::unordered_map<std::string,
                                     std::pair<std::string, int64_t>> url_cache_;

    static IscUrl parse(const std::string& url) {
        const std::string prefix = "isc://";
        if (url.rfind(prefix, 0) != 0) throw std::runtime_error("not an isc:// url");
        std::string rest = url.substr(prefix.size());
        size_t at = rest.find('@');
        size_t q = rest.find('?');
        if (at == std::string::npos || q == std::string::npos || at > q)
            throw std::runtime_error("invalid isc url");
        std::string cred = rest.substr(0, at);
        std::string authority = rest.substr(at + 1, q - at - 1);
        std::string query = rest.substr(q + 1);
        size_t colon = cred.find(':');
        if (colon == std::string::npos) throw std::runtime_error("invalid isc credentials");
        IscUrl u;
        u.app_key = cred.substr(0, colon);
        u.app_secret = cred.substr(colon + 1);
        size_t pcolon = authority.find(':');
        if (pcolon == std::string::npos) throw std::runtime_error("invalid isc host");
        u.host = authority.substr(0, pcolon);
        u.port = authority.substr(pcolon + 1);
        auto qs = parse_query(query);
        u.camera_index = qs.count("cameraIndexCode") ? qs["cameraIndexCode"] : "";
        u.stream_type = qs.count("streamType") ? std::atoi(qs["streamType"].c_str()) : 0;
        u.protocol = qs.count("protocol") && !qs["protocol"].empty()
                         ? qs["protocol"]
                         : "rtsp";
        u.transcode = qs.count("transcode") ? std::atoi(qs["transcode"].c_str()) : -1;
        u.videotype = qs.count("videotype") ? qs["videotype"] : "";
        u.https = qs.count("https") ? std::atoi(qs["https"].c_str()) : -1;
        if (u.camera_index.empty()) throw std::runtime_error("isc cameraIndexCode missing");
        return u;
    }

    static std::map<std::string, std::string> parse_query(const std::string& qs) {
        std::map<std::string, std::string> out;
        size_t start = 0;
        while (start <= qs.size()) {
            size_t amp = qs.find('&', start);
            std::string part = qs.substr(
                start, amp == std::string::npos ? std::string::npos : amp - start);
            size_t eq = part.find('=');
            if (eq != std::string::npos) {
                out[part.substr(0, eq)] = part.substr(eq + 1);
            }
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
        return out;
    }

    static std::string random_uuid() {
        static std::mt19937_64 rng{std::random_device{}()};
        static std::mutex m;
        std::lock_guard<std::mutex> lk(m);
        uint64_t a = rng();
        uint64_t b = rng();
        char buf[37];
        std::snprintf(buf, sizeof(buf),
                      "%08x-%04x-4%03x-%04x-%012llx",
                      static_cast<unsigned>(a >> 32),
                      static_cast<unsigned>((a >> 16) & 0xffff),
                      static_cast<unsigned>(a & 0xfff),
                      static_cast<unsigned>(0x8000 | ((b >> 48) & 0x3fff)),
                      static_cast<unsigned long long>(b & 0xffffffffffffULL));
        return std::string(buf);
    }

    static std::string random_hex(size_t bytes) {
        std::random_device rd;
        std::ostringstream ss;
        for (size_t i = 0; i < bytes; ++i) {
            unsigned int v = rd() & 0xff;
            ss << std::hex << std::setw(2) << std::setfill('0') << v;
        }
        return ss.str();
    }

    static HttpResponse https_post(const std::string& host, const std::string& port,
                                   const std::string& target, const std::string& body,
                                   const std::map<std::string, std::string>& headers,
                                   int timeout_ms) {
        HttpResponse result;
        try {
            asio::io_context ioc;
            asio::ssl::context ctx(asio::ssl::context::tls_client);
            ctx.set_verify_mode(asio::ssl::verify_none);
            asio::ssl::stream<beast::tcp_stream> stream(ioc, ctx);
            tcp::resolver resolver(ioc);
            std::string connect_host = host;
            std::string connect_port = port;
            const char* proxy = std::getenv("AIDETECT_ISC_PROXY");
            if (proxy && *proxy) {
                std::string p(proxy);
                size_t colon = p.rfind(':');
                if (colon != std::string::npos) {
                    connect_host = p.substr(0, colon);
                    connect_port = p.substr(colon + 1);
                    log_info("ISC using TCP proxy " + connect_host + ":" +
                             connect_port + " for host " + host);
                }
            }
            auto const results = resolver.resolve(connect_host, connect_port);
            beast::get_lowest_layer(stream).expires_after(
                std::chrono::milliseconds(timeout_ms));
            beast::get_lowest_layer(stream).connect(results);
            SSL_set_tlsext_host_name(stream.native_handle(), host.c_str());
            beast::get_lowest_layer(stream).expires_after(
                std::chrono::milliseconds(timeout_ms));
            stream.handshake(asio::ssl::stream_base::client);
            http::request<http::string_body> req{http::verb::post, target, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "aidetect/0.2");
            for (auto& kv : headers) req.set(kv.first, kv.second);
            req.body() = body;
            req.prepare_payload();
            beast::get_lowest_layer(stream).expires_after(
                std::chrono::milliseconds(timeout_ms));
            http::write(stream, req);
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            // Hard deadline for the response: a wedged ISC gateway used to
            // block this read forever and the task stopped retrying entirely.
            beast::get_lowest_layer(stream).expires_after(
                std::chrono::milliseconds(timeout_ms));
            http::read(stream, buffer, res);
            result.status = res.result_int();
            result.body = res.body();
            beast::error_code ec;
            stream.shutdown(ec);
        } catch (const std::exception& e) {
            result.status = -1;
            result.body = e.what();
        }
        return result;
    }
};

}  // namespace aidetect
