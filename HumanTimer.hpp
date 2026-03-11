#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  HumanTimer.hpp  —  GD AI Solver Extreme
//  Adds realistic, human-like timing variance to jump inputs.
//
//  Design goals
//  ────────────
//  • Non-critical jumps: add a small random delay (0 … maxVarianceMs) so
//    replays don't look mechanical.
//  • Frame-perfect sections: variance is clamped to ZERO so the bot never
//    misses a tight gap.
//  • Thread-safe: state is local to each call — no shared mutable globals.
// ─────────────────────────────────────────────────────────────────────────────
#include <random>
#include <chrono>
#include <cstdint>

namespace GDAI {

// ─── HumanTimer ─────────────────────────────────────────────────────────────
class HumanTimer {
public:
    // ctor — seeds the PRNG once per instance (one instance per PlayLayer session)
    HumanTimer()
        : m_rng(std::random_device{}())
        , m_pendingDelayMs(0)
        , m_isCounting(false)
        , m_elapsedMs(0.0)
    {}

    // ─── API ────────────────────────────────────────────────────────────────

    /**
     * Schedule a delayed click.
     * @param maxVarianceMs  Upper bound for random delay in milliseconds.
     *                       Pass 0 for a frame-perfect (no delay) click.
     */
    void scheduleClick(int maxVarianceMs) {
        if (maxVarianceMs <= 0) {
            // Frame-perfect — fire immediately
            m_pendingDelayMs = 0;
        } else {
            std::uniform_int_distribution<int> dist(0, maxVarianceMs);
            m_pendingDelayMs = dist(m_rng);
        }
        m_elapsedMs  = 0.0;
        m_isCounting = true;
    }

    /**
     * Call once per game-update frame with the real delta-time.
     * Returns true when the delayed click should fire.
     *
     * @param dtSeconds  Delta-time in seconds (from Cocos2d scheduler / update()).
     *                   Using real dt correctly handles mobile lag & FPS drops.
     */
    bool tick(float dtSeconds) {
        if (!m_isCounting) return false;

        m_elapsedMs += dtSeconds * 1000.0;

        if (m_elapsedMs >= static_cast<double>(m_pendingDelayMs)) {
            m_isCounting = false;
            m_elapsedMs  = 0.0;
            return true;  // fire the click
        }
        return false;
    }

    /** Cancel any pending scheduled click. */
    void cancel() {
        m_isCounting     = false;
        m_pendingDelayMs = 0;
        m_elapsedMs      = 0.0;
    }

    bool isCounting() const { return m_isCounting; }

private:
    std::mt19937       m_rng;
    int                m_pendingDelayMs;
    bool               m_isCounting;
    double             m_elapsedMs;      // use double to accumulate small dt values
};

} // namespace GDAI
