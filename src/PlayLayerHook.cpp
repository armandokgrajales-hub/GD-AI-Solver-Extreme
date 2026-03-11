// ─────────────────────────────────────────────────────────────────────────────
//  hooks/PlayLayerHook.cpp  —  GD AI Solver Extreme
//
//  Hooks PlayLayer lifecycle + update loop to drive the AI engine.
//  All GD/Cocos2d object access stays on the main thread — zero extra threads.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../AIEngine.hpp"

using namespace geode::prelude;
using namespace GDAI;

struct GDAISolverPlayLayer : Modify<GDAISolverPlayLayer, PlayLayer> {

    // ── Level load (first run and after retry) ───────────────────────────────
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        AIEngine::get().onSessionStart();
        return true;
    }

    // ── Retry / death restart ─────────────────────────────────────────────────
    void resetLevel() {
        AIEngine::get().onSessionEnd();
        PlayLayer::resetLevel();
        AIEngine::get().onSessionStart();
    }

    // ── Player quits the level ────────────────────────────────────────────────
    void onQuit() {
        AIEngine::get().onSessionEnd();
        PlayLayer::onQuit();
    }

    // ── Main game loop ────────────────────────────────────────────────────────
    //  dt is real elapsed seconds — passed straight to the engine so that
    //  HumanTimer accumulates wall-clock time correctly on mobile under lag.
    void update(float dt) {
        PlayLayer::update(dt);          // GD logic runs first — positions are fresh

        // Critical null-guard: m_player1 can be nullptr during load/unload
        // transitions on Android. Accessing it without this check = SIGSEGV.
        if (!m_player1) return;

        AIEngine::get().update(this, dt);
    }
};
