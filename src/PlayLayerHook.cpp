// ─────────────────────────────────────────────────────────────────────────────
//  hooks/PlayLayerHook.cpp  —  GD AI Solver Extreme
//
//  Hooks into PlayLayer's lifecycle and update loop to drive the AI engine.
//
//  Thread-safety notes (Android / mobile)
//  ───────────────────────────────────────
//  • All Cocos2d / GD object access happens ONLY on the main thread (inside the
//    hooked virtual functions which GD always calls on the GL/main thread).
//  • AIEngine::update() is O(N) bounded by the look-ahead window — it will NOT
//    stall the main thread on typical levels.
//  • We never spawn extra threads here; the engine is intentionally single-
//    threaded to keep it crash-free on mobile.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../AIEngine.hpp"

using namespace geode::prelude;
using namespace GDAI;

// ─── PlayLayer hook ───────────────────────────────────────────────────────────
struct GDAISolverPlayLayer : Modify<GDAISolverPlayLayer, PlayLayer> {

    // ── Called when a level loads (first run or retry) ───────────────────────
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        // Always call the original first — Geode pattern.
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        AIEngine::get().onSessionStart();
        return true;
    }

    // ── Called on death / restart (resetLevel is the canonical restart hook) ─
    void resetLevel() {
        // End the previous session cleanly before the original code runs.
        AIEngine::get().onSessionEnd();
        PlayLayer::resetLevel();
        // Start a fresh session after reset.
        AIEngine::get().onSessionStart();
    }

    // ── Called when the player quits mid-level ────────────────────────────────
    void onQuit() {
        AIEngine::get().onSessionEnd();
        PlayLayer::onQuit();
    }

    // ── Main update loop — THE AI TICK ───────────────────────────────────────
    //  GD calls update(float dt) every visual frame (~60 fps on desktop,
    //  variable on mobile under load).
    //  dt is already in seconds and reflects real elapsed time — we pass it
    //  straight to AIEngine so the HumanTimer accumulates correctly under lag.
    void update(float dt) {
        // Run original game logic first so m_player1 positions are current.
        PlayLayer::update(dt);

        // ── Null guard — critical for mobile stability ──────────────────────
        //  On Android, during level load/unload transitions, m_player1 can be
        //  nullptr for one or more frames.  Accessing it without this check
        //  causes a SIGSEGV / native crash.
        if (!m_player1) return;

        // ── AI tick (main-thread only, no mutexes needed here) ───────────────
        AIEngine::get().update(this, dt);

        // ── Optional: debug hitbox overlay ───────────────────────────────────
#ifdef GEODE_DEBUG
        if (Mod::get()->getSavedValue<bool>("show-debug-hitboxes", false)) {
            // Draw player hitbox outline using Cocos2d draw primitives
            const CCRect r = m_player1->getObjectRect();
            // (Geode debug draw helpers may vary by version — log instead)
            geode::log::debug("[GD-AI DBG] Player AABB x={:.1f} y={:.1f} w={:.1f} h={:.1f}",
                r.origin.x, r.origin.y, r.size.width, r.size.height);
        }
#endif
    }

    // ── Intercept death to stop AI immediately ───────────────────────────────
    //  playerDied() is called synchronously before resetLevel on death.
    void playerDied(PlayerObject* player) {
        // Stop any pending jump from firing after death (would cause ghost input)
        AIEngine::get().onSessionEnd();
        PlayLayer::playerDied(player);
    }
};
