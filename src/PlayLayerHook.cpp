// ─────────────────────────────────────────────────────────────────────────────
//  hooks/PlayLayerHook.cpp  —  GD AI Solver Extreme
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "../AIEngine.hpp"

using namespace geode::prelude;
using namespace GDAI;

struct GDAISolverPlayLayer : Modify<GDAISolverPlayLayer, PlayLayer> {

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        AIEngine::get().onSessionStart();
        return true;
    }

    void resetLevel() {
        AIEngine::get().onSessionEnd();
        PlayLayer::resetLevel();
        AIEngine::get().onSessionStart();
    }

    void onQuit() {
        AIEngine::get().onSessionEnd();
        PlayLayer::onQuit();
    }

    // dt = real elapsed seconds — passed straight through so HumanTimer
    // accumulates wall-clock time correctly even under mobile lag.
    void update(float dt) {
        PlayLayer::update(dt);
        // Critical null-guard: m_player1 can be null during Android transitions.
        if (!m_player1) return;
        AIEngine::get().update(this, dt);
    }
};

