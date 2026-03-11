#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  AIEngine.hpp  —  GD AI Solver Extreme
//
//  Responsible for:
//    1. Scanning objects ahead of the player (hitbox-only, ignores deco).
//    2. Predicting collisions via lightweight trajectory simulation.
//    3. Deciding WHEN to jump (and scheduling it through HumanTimer).
//    4. Thread-safe state so analysis never corrupts the main GD thread.
//
//  Architecture note
//  ─────────────────
//  Analysis runs on the MAIN THREAD inside PlayLayer::update() to keep full
//  access to Cocos2d objects (they are not thread-safe).  The work is kept
//  O(n) and bounded by the look-ahead window to stay within one frame budget.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <atomic>
#include <mutex>
#include <vector>
#include "utils/HumanTimer.hpp"

using namespace geode::prelude;

namespace GDAI {

// ─── HazardInfo ─────────────────────────────────────────────────────────────
// Lightweight POD describing a single threat detected ahead of the player.
struct HazardInfo {
    float   x;              // world-space left edge of hazard
    float   y;
    float   width;
    float   height;
    bool    isFramePerfect; // true when < framePerfectThreshold pixels away
};

// ─── JumpDecision ───────────────────────────────────────────────────────────
struct JumpDecision {
    bool  shouldJump     = false;
    bool  framePerfect   = false; // bypass human variance
    float distanceToEdge = 0.f;
};

// ─── AIEngine ───────────────────────────────────────────────────────────────
class AIEngine {
public:
    // ── Singleton access ────────────────────────────────────────────────────
    static AIEngine& get() {
        static AIEngine instance;
        return instance;
    }

    // ── Session lifecycle ────────────────────────────────────────────────────
    /** Call when a new level session begins (resetLevel / init). */
    void onSessionStart();
    /** Call when the session ends (die, quit, complete). */
    void onSessionEnd();

    // ── Per-frame update ─────────────────────────────────────────────────────
    /**
     * Main entry point — call from PlayLayer::update(float dt).
     *
     * @param layer  The current PlayLayer.
     * @param dt     Delta-time in seconds (raw, not speed-scaled).
     */
    void update(PlayLayer* layer, float dt);

    // ── State queries ─────────────────────────────────────────────────────────
    bool isEnabled()  const { return m_enabled; }
    void setEnabled(bool v) { m_enabled = v; }

    bool isRunning()  const { return m_sessionActive; }

private:
    AIEngine()  = default;
    ~AIEngine() = default;
    AIEngine(const AIEngine&) = delete;
    AIEngine& operator=(const AIEngine&) = delete;

    // ── Internal helpers ────────────────────────────────────────────────────
    /**
     * Scan m_objects in layer and populate m_hazards with objects whose
     * hitboxes overlap the look-ahead window.  Decorations are skipped.
     * Called at most once every SCAN_INTERVAL_FRAMES frames.
     */
    void scanHazards(PlayLayer* layer, const CCPoint& playerPos);

    /**
     * Given the current hazard list, decide whether the player should jump.
     * Uses a simple two-phase simulation:
     *   Phase 1 (no jump): project current trajectory → collision? → must jump.
     *   Phase 2 (with jump): project jump trajectory → no collision? → jump now.
     */
    JumpDecision computeDecision(PlayerObject* player, const CCPoint& playerPos);

    /**
     * Returns true for solid/hazard objects we care about.
     * Filters out decorations, triggers, animated BGs, etc.
     */
    static bool isHazard(GameObject* obj);

    /**
     * Simulate whether the player's bounding box at (px, py) travelling with
     * velocity (vx, vy) will intersect 'rect' within 'steps' steps of 'dt'.
     */
    static bool trajectoryHits(float px, float py,
                                float vx, float vy,
                                const CCRect& rect,
                                int steps, float dt);

    // ── State ───────────────────────────────────────────────────────────────
    std::atomic<bool>     m_enabled       { false };
    std::atomic<bool>     m_sessionActive { false };

    // Hazard cache — rebuilt every SCAN_INTERVAL_FRAMES frames
    std::vector<HazardInfo> m_hazards;
    int                     m_frameCounter { 0 };
    static constexpr int    SCAN_INTERVAL_FRAMES = 3; // rescan every 3 frames

    // Human-timing helper
    HumanTimer  m_humanTimer;
    bool        m_jumpScheduled   { false };
    bool        m_holdingJump     { false };

    // Settings cache (re-read each session start for performance)
    float m_lookahead           { 320.f };
    float m_framePerfectThresh  { 22.f  };
    int   m_humanVarianceMs     { 15    };
};

} // namespace GDAI
