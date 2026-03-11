#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  HumanTimer.hpp  —  GD AI Solver Extreme
//  Accumulates real delta-time to produce human-like jump delays.
//  No Geode/Cocos2d dependencies — plain C++20.
// ─────────────────────────────────────────────────────────────────────────────
#include <random>
#include <cstdint>

namespace GDAI {

class HumanTimer {
public:
    HumanTimer()
        : m_rng(std::random_device{}())
        , m_pendingDelayMs(0)
        , m_isCounting(false)
        , m_elapsedMs(0.0)
    {}

    // Schedule a click with up to maxVarianceMs of random delay.
    // Pass 0 for an immediate (frame-perfect) click.
    void scheduleClick(int maxVarianceMs) {
        if (maxVarianceMs <= 0) {
            m_pendingDelayMs = 0;
        } else {
            std::uniform_int_distribution<int> dist(0, maxVarianceMs);
            m_pendingDelayMs = dist(m_rng);
        }
        m_elapsedMs  = 0.0;
        m_isCounting = true;
    }

    // Call every frame with real delta-time (seconds).
    // Returns true exactly once when the delay has elapsed.
    // Using real dt means mobile lag spikes never cause missed/double clicks.
    bool tick(float dtSeconds) {
        if (!m_isCounting) return false;
        m_elapsedMs += static_cast<double>(dtSeconds) * 1000.0;
        if (m_elapsedMs >= static_cast<double>(m_pendingDelayMs)) {
            m_isCounting = false;
            m_elapsedMs  = 0.0;
            return true;
        }
        return false;
    }

    void cancel() {
        m_isCounting     = false;
        m_pendingDelayMs = 0;
        m_elapsedMs      = 0.0;
    }

    bool isCounting() const { return m_isCounting; }

private:
    std::mt19937 m_rng;
    int          m_pendingDelayMs;
    bool         m_isCounting;
    double       m_elapsedMs;
};

} // namespace GDAI

