#include "server.hpp"

#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>

static void crash_handler(int sig) {
    void* frames[48];
    int n = backtrace(frames, 48);
    const char* msg = "\n=== FATAL SIGNAL, backtrace ===\n";
    write(STDERR_FILENO, msg, strlen(msg));
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    _exit(128 + sig);
}

int main(int argc, char** argv) {
    using namespace aidetect;
    if (argc >= 2 && std::string(argv[1]) == "--dump-only") {
        std::string url, out;
        int pre = 5, post = 5;
        for (int i = 2; i + 1 < argc; ++i) {
            std::string k = argv[i];
            if (k == "--url") url = argv[++i];
            else if (k == "--out") out = argv[++i];
            else if (k == "--pre") pre = std::atoi(argv[++i]);
            else if (k == "--post") post = std::atoi(argv[++i]);
        }
        if (url.empty() || out.empty()) return 2;
        try {
            sail::Handle handle(0);
            sail::Decoder decoder(url, true, 0);
            if (!decoder.is_opened()) return 3;
            // dump() dumps the decoder's buffered GOP. Prime the decoder with
            // a few frames first; a freshly constructed decoder has no packets.
            decoder.enable_dump(std::max(10, pre + post + 2));
            sail::BMImage frame;
            for (int i = 0; i < 30; ++i) {
                int rr = decoder.read(handle, frame);
                if (rr != 0) break;
            }
            int rc = decoder.dump(std::max(1, pre), std::max(1, post), out);
            // dump() keeps writing while decoding continues; keep feeding the
            // decoder for the post window before closing the process.
            for (int i = 0; i < (std::max(1, post) + 2) * 25; ++i) {
                int rr = decoder.read(handle, frame);
                if (rr != 0) break;
            }
            // dump writes asynchronously in libsail; keep the process and
            // decoder alive briefly so the MP4 trailer is flushed.
            sleep(3);
            return rc == 0 ? 0 : 4;
        } catch (...) {
            return 5;
        }
    }
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGBUS, crash_handler);
    AppConfig cfg = AppConfig::from_env();
    if (cfg.jwt_secret.empty()) {
        log_warn("AIDETECT_JWT_SECRET is empty; all authenticated requests will be rejected");
    }
    // A previous process may have left ffmpeg gateway children behind (they
    // were spawned through a shell and got reparented to init). They hold the
    // local UDP ports and ISC preview sessions, which is why a restarted AI
    // service could only open a handful of channels. Clean only our gateway
    // command line, never the label/video workers.
    if (!std::getenv("AIDETECT_NO_GATEWAY_CLEANUP")) {
        int rc = std::system(
            "pkill -f 'ffmpeg .*udp://127.0.0.1:22' 2>/dev/null");
        if (rc == 0) log_info("cleaned stale ffmpeg gateway processes");
    }
    try {
        ModelScheduler model(cfg);
        DecodePool decode_pool;
        StreamHub hub;
        Aggregator aggregator(hub);
        Jwt jwt(cfg.jwt_secret);
        TaskManager manager(cfg, model, decode_pool, hub, aggregator, jwt);
        std::thread([&manager]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            while (true) {
                // Startup reconcile also starts status=1 tasks; subsequent
                // calls only re-check running-task configs.  Without this
                // loop ROI/algorithm edits in the Web UI never reached a
                // running Task (reconcile used to be one-shot).
                manager.reconcile_from_backend();
                manager.supervise_stalled_tasks();
                // 2s keeps a saved ROI/algorithm edit visually "live" while
                // still being cheap (the backend query is local and only
                // 36 task configs are compared).
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }).detach();
    // Resource watchdog.  Long preview sessions slowly accumulate BM (dmabuf)
    // buffers; when the BM heap runs dry the decoders stop getting frames and
    // tasks stay dark until the process restarts (seen 2026-11-13: 925 dmabuf
    // handles vs 117 at start, 19 of 32 tasks dark).  Restart the process
    // before that happens: docker `--restart unless-stopped` brings it back in
    // seconds and the tasks are reconciled from the backend.
    std::thread([&manager]() {
        const int max_dmabuf =
            std::getenv("AIDETECT_WATCHDOG_DMABUF")
                ? std::atoi(std::getenv("AIDETECT_WATCHDOG_DMABUF"))
                : 1000;
        const int max_rss_mb =
            std::getenv("AIDETECT_WATCHDOG_RSS_MB")
                ? std::atoi(std::getenv("AIDETECT_WATCHDOG_RSS_MB"))
                : 2000;
        if (max_dmabuf <= 0 && max_rss_mb <= 0) return;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            int dmabuf = 0;
            try {
                for (auto& e : std::filesystem::directory_iterator("/proc/self/fd")) {
                    std::error_code ec;
                    auto tgt = std::filesystem::read_symlink(e.path(), ec);
                    if (!ec && tgt.string().find("dmabuf") != std::string::npos)
                        ++dmabuf;
                }
            } catch (...) {
            }
            long rss_mb = 0;
            {
                std::ifstream st("/proc/self/status");
                std::string line;
                while (std::getline(st, line)) {
                    if (line.rfind("VmRSS:", 0) == 0) {
                        rss_mb = std::atol(line.c_str() + 6) / 1024;
                        break;
                    }
                }
            }
            static int sample = 0;
            if (++sample % 20 == 0)
                log_info("resource: dmabuf=" + std::to_string(dmabuf) +
                         " rss_mb=" + std::to_string(rss_mb));
            const bool over_dmabuf = max_dmabuf > 0 && dmabuf > max_dmabuf;
            const bool over_rss = max_rss_mb > 0 && rss_mb > max_rss_mb;
            if (over_dmabuf || over_rss) {
                log_warn("resource watchdog: dmabuf=" + std::to_string(dmabuf) +
                         " rss_mb=" + std::to_string(rss_mb) +
                         " over limit, restarting process to release BM buffers");
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                ::_exit(0);
            }
        }
    }).detach();

        HttpServer server(cfg, jwt, manager, hub);
        server.run();
    } catch (const std::exception& e) {
        log_error(std::string("fatal: ") + e.what());
        return 1;
    }
    return 0;
}
