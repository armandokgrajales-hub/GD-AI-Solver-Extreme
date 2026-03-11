#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  AIEngine.hpp  —  GD AI Solver Extreme
//
//  IMPORTANT: Do NOT include <Geode/modify/...> headers here.
//  Modify headers must only appear in .cpp files.
//  This header only uses forward-declared GD types via <Geode/Geode.hpp>.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <atomic>
#include <vector>
#include "utils/HumanTimer.hpp"

using namespace geode::prelude;

namespace GDAI {

// Lightweight description of one hazard object ahead of the player
struct HazardInfo {
    float x, y, width, height;
    bool  isFramePerfect; // true when within the frame-perfect threshold
};

struct JumpDecision {
    bool  shouldJump     = false;
    bool  framePerfect   = false;
    float distanceToEdge = 0.f;
};

class AIEngine {
public:
    static AIEngine& get() {
        static AIEngine instance;
        return instance;
    }

    void onSessionStart();
    void onSessionEnd();

    // Called every frame from PlayLayer::update(float dt)
    void update(PlayLayer* layer, float dt);

    bool isEnabled()      const { return m_enabled.load(); }
    void setEnabled(bool v)     { m_enabled.store(v); }
    bool isRunning()      const { return m_sessionActive.load(); }

private:
    AIEngine()  = default;
    ~AIEngine() = default;
    AIEngine(const AIEngine&)            = delete;
    AIEngine& operator=(const AIEngine&) = delete;

    void         scanHazards   (PlayLayer* layer, const CCPoint& playerPos);
    JumpDecision computeDecision(PlayerObject* player, const CCPoint& playerPos);

    static bool isHazard(GameObject* obj);
    static bool trajectoryHits(float px, float py,
                                float vx, float vy,
                                const CCRect& rect,
                                int steps, float dt);

    std::atomic<bool>     m_enabled       { false };
    std::atomic<bool>     m_sessionActive { false };

    std::vector<HazardInfo> m_hazards;
    int                     m_frameCounter { 0 };
    static constexpr int    SCAN_INTERVAL  = 3;

    HumanTimer m_humanTimer;
    bool       m_jumpScheduled { false };
    bool       m_holdingJump   { false };

    // Cached settings (refreshed each session)
    float m_lookahead          { 320.f };
    float m_framePerfectThresh { 22.f  };
    int   m_humanVarianceMs    { 15    };
};

} // namespace GDAI

