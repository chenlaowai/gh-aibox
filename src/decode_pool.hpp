#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common.hpp"
#include <sail/decoder_multi.h>

namespace aidetect {

// Four MultiDecoder groups, nine channels each (4 x 9 = 36 tasks). MultiDecoder
// owns one decode thread per channel, so we do not open 36 independent
// sail::Decoder objects (which overloaded VPU/JPU on this box).
class DecodePool {
public:
    static constexpr int kGroups = 4;
    static constexpr int kSlots = 9;

    DecodePool() {
        for (int g = 0; g < kGroups; ++g) {
            auto group = std::make_unique<Group>();
            group->md = std::make_unique<sail::MultiDecoder>(10, 0, 0);
            group->md->set_read_timeout(2);
            groups_.push_back(std::move(group));
        }
        log_info("DecodePool: 4 MultiDecoder groups x 9 channels");
    }
    ~DecodePool() = default;

    int add(int group, int slot, const std::string& source) {
        if (!valid(group, slot)) return -1;
        auto& g = *groups_[group];
        std::lock_guard<std::mutex> lk(g.mu);
        if (g.channels[slot] >= 0) return g.channels[slot];
        std::lock_guard<std::mutex> hw(decode_hw_mutex());
        int loop = -1;
        int ch = g.md->add_channel(source, 0, loop);
        if (ch < 0) {
            log_error("DecodePool add failed group=" + std::to_string(group) +
                      " slot=" + std::to_string(slot));
            return -1;
        }
        // Wait until the channel thread has opened/probed the source before the
        // next task starts another channel. This serializes the expensive JPU
        // initialization that otherwise races under 36 simultaneous starts.
        for (int i = 0; i < 60; ++i) {
            auto s = g.md->get_frame_shape(ch);
            if (s.size() >= 4 && s[2] > 0 && s[3] > 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        g.channels[slot] = ch;
        log_info("DecodePool add g=" + std::to_string(group) + " slot=" +
                 std::to_string(slot) + " ch=" + std::to_string(ch));
        return ch;
    }

    void remove(int group, int slot) {
        if (!valid(group, slot)) return;
        auto& g = *groups_[group];
        std::lock_guard<std::mutex> lk(g.mu);
        int ch = g.channels[slot];
        if (ch < 0) return;
        g.channels[slot] = -1;
        std::lock_guard<std::mutex> hw(decode_hw_mutex());
        g.md->del_channel(ch);
        log_info("DecodePool remove g=" + std::to_string(group) + " slot=" +
                 std::to_string(slot) + " ch=" + std::to_string(ch));
    }

    // Destroying a channel while other channels are still decoding can trigger
    // Jpu_GetError in libsophon. Rebuild the whole group only when it is idle.
    void reset_group(int group) {
        if (group < 0 || group >= kGroups) return;
        auto& g = *groups_[group];
        std::lock_guard<std::mutex> lk(g.mu);
        std::lock_guard<std::mutex> hw(decode_hw_mutex());
        g.md.reset();
        g.md = std::make_unique<sail::MultiDecoder>(10, 0, 0);
        g.md->set_read_timeout(2);
        g.channels.fill(-1);
        log_info("DecodePool reset group=" + std::to_string(group));
    }

    int read(int group, int slot, sail::BMImage& image) {
        if (!valid(group, slot)) return -1;
        int ch;
        {
            auto& g = *groups_[group];
            std::lock_guard<std::mutex> lk(g.mu);
            ch = g.channels[slot];
        }
        if (ch < 0) return -1;
        // Discard frames already queued before blocking for a fresh one.
        // Otherwise the alarm image can be tens of seconds behind the live
        // stream while SAIL dump starts at the current live position.
        sail::BMImage stale;
        for (int i = 0; i < 80; ++i) {
            int r = groups_[group]->md->read(ch, stale, 0);
            if (r != 0 || stale.width() <= 0 || stale.height() <= 0) break;
        }
        return groups_[group]->md->read(ch, image, 1);
    }

    std::vector<int> shape(int group, int slot) {
        if (!valid(group, slot)) return {};
        int ch;
        {
            auto& g = *groups_[group];
            std::lock_guard<std::mutex> lk(g.mu);
            ch = g.channels[slot];
        }
        if (ch < 0) return {};
        return groups_[group]->md->get_frame_shape(ch);
    }

    void reconnect(int group, int slot) {
        if (!valid(group, slot)) return;
        int ch;
        {
            auto& g = *groups_[group];
            std::lock_guard<std::mutex> lk(g.mu);
            ch = g.channels[slot];
        }
        if (ch >= 0) groups_[group]->md->reconnect(ch);
    }

    void enable_dump(int group, int slot, int seconds) {
        if (!valid(group, slot)) return;
        int ch;
        {
            auto& g = *groups_[group];
            std::lock_guard<std::mutex> lk(g.mu);
            ch = g.channels[slot];
        }
        if (ch >= 0) groups_[group]->md->enable_dump(ch, seconds);
    }

    int dump(int group, int slot, int pre, int post, std::string& path) {
        if (!valid(group, slot)) return -1;
        int ch;
        {
            auto& g = *groups_[group];
            std::lock_guard<std::mutex> lk(g.mu);
            ch = g.channels[slot];
        }
        if (ch < 0) return -1;
        return groups_[group]->md->dump(ch, pre, post, path);
    }

private:
    struct Group {
        std::unique_ptr<sail::MultiDecoder> md;
        std::mutex mu;
        std::array<int, kSlots> channels{-1, -1, -1, -1, -1, -1, -1, -1, -1};
    };

    static bool valid(int group, int slot) {
        return group >= 0 && group < kGroups && slot >= 0 && slot < kSlots;
    }

    std::vector<std::unique_ptr<Group>> groups_;
};

}  // namespace aidetect
