// ─────────────────────────────────────────────────────────────────────────────
//  AIEngine.cpp  —  GD AI Solver Extreme
// ─────────────────────────────────────────────────────────────────────────────
#include "AIEngine.hpp"

using namespace geode::prelude;
using namespace GDAI;

// ── GD physics constants (GD 2.2, speed x1) ──────────────────────────────────
// GD physics ticks at 240 Hz internally regardless of visual FPS.
static constexpr float PLAYER_HALF  = 10.f;   // half-size of player hitbox (px)
static constexpr float JUMP_VY      = 11.18f;  // jump impulse in px/tick
static constexpr float GRAVITY      = -0.958f; // gravity per tick
static constexpr float SPEED_X      = 8.37f;   // player X speed px/tick (speed x1)
static constexpr float TICKS_PER_S  = 240.f;

// ── Session lifecycle ─────────────────────────────────────────────────────────
void AIEngine::onSessionStart() {
    auto* mod = Mod::get();
    m_lookahead          = mod->getSavedValue<float>("lookahead-distance",      320.f);
    m_framePerfectThresh = mod->getSavedValue<float>("frame-perfect-threshold",  22.f);
    m_humanVarianceMs    = mod->getSavedValue<int>  ("human-variance-ms",         15 );
    m_enabled.store(       mod->getSavedValue<bool> ("ai-enabled",             false));

    m_hazards.clear();
    m_humanTimer.cancel();
    m_frameCounter  = 0;
    m_jumpScheduled = false;
    m_holdingJump   = false;
    m_sessionActive.store(true);

    geode::log::info("[GD-AI] Session start | enabled={} lookahead={} fpThresh={} variance={}ms",
        m_enabled.load(), m_lookahead, m_framePerfectThresh, m_humanVarianceMs);
}

void AIEngine::onSessionEnd() {
    m_sessionActive.store(false);
    m_hazards.clear();
    m_humanTimer.cancel();
    m_holdingJump   = false;
    m_jumpScheduled = false;
}

// ── Per-frame update ──────────────────────────────────────────────────────────
void AIEngine::update(PlayLayer* layer, float dt) {
    if (!m_enabled.load() || !m_sessionActive.load()) return;
    if (!layer) return;

    PlayerObject* player = layer->m_player1;
    if (!player) return;

    const CCPoint pos = player->getPosition();

    // Rescan hazards every SCAN_INTERVAL frames (not every frame — saves mobile CPU)
    if (++m_frameCounter >= SCAN_INTERVAL) {
        m_frameCounter = 0;
        scanHazards(layer, pos);
    }

    // Tick the human-delay timer — fires when accumulated real-time >= scheduled delay
    if (m_jumpScheduled && m_humanTimer.tick(dt)) {
        m_jumpScheduled = false;
        m_holdingJump   = true;
        player->pushButton(PlayerButton::Jump);
        return;
    }

    // Release the button on the very next frame after pressing (simulates a tap)
    if (m_holdingJump) {
        m_holdingJump = false;
        player->releaseButton(PlayerButton::Jump);
        return;
    }

    // Don't queue another jump while one is already pending
    if (m_jumpScheduled || m_humanTimer.isCounting()) return;

    JumpDecision decision = computeDecision(player, pos);
    if (decision.shouldJump) {
        int variance = decision.framePerfect ? 0 : m_humanVarianceMs;
        m_humanTimer.scheduleClick(variance);
        m_jumpScheduled = true;
        geode::log::debug("[GD-AI] Jump | fp={} dist={:.1f}px delay={}ms",
            decision.framePerfect, decision.distanceToEdge, variance);
    }
}

// ── Hazard scan ───────────────────────────────────────────────────────────────
void AIEngine::scanHazards(PlayLayer* layer, const CCPoint& playerPos) {
    m_hazards.clear();

    CCArray* objects = layer->m_objects;
    if (!objects) return;

    const float xLeft  = playerPos.x - 20.f;
    const float xRight = playerPos.x + m_lookahead;

    m_hazards.reserve(32);

    for (int i = 0, n = static_cast<int>(objects->count()); i < n; ++i) {
        auto* obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (!obj || !isHazard(obj)) continue;

        const CCRect r = obj->getObjectRect();
        // Cull objects outside the look-ahead strip
        if (r.getMaxX() < xLeft || r.getMinX() > xRight) continue;

        HazardInfo info;
        info.x              = r.origin.x;
        info.y              = r.origin.y;
        info.width          = r.size.width;
        info.height         = r.size.height;
        info.isFramePerfect = (r.origin.x - playerPos.x) <= m_framePerfectThresh;
        m_hazards.push_back(info);
    }
}

// ── Decision logic ────────────────────────────────────────────────────────────
JumpDecision AIEngine::computeDecision(PlayerObject* player,
                                        const CCPoint& playerPos)
{
    JumpDecision result;
    if (m_hazards.empty()) return result;

    // Use the known GD speed constant for X; read Y velocity from the player.
    // m_yVelocity is a well-established binding in GD 2.2.
    // We deliberately avoid m_xVelocity as it may be 0 when speed is applied
    // by the level rather than stored on the player object.
    const float vx = SPEED_X;
    const float vy = static_cast<float>(player->m_yVelocity);

    constexpr int   STEPS  = 60;
    constexpr float SIM_DT = 1.f / TICKS_PER_S;

    // Phase 1: will we die WITHOUT jumping?
    bool  willDie = false;
    float dangerX = 0.f;
    bool  isFP    = false;

    for (const auto& h : m_hazards) {
        CCRect hr { h.x, h.y, h.width, h.height };
        if (trajectoryHits(playerPos.x, playerPos.y, vx, vy, hr, STEPS, SIM_DT)) {
            willDie = true;
            dangerX = h.x;
            isFP    = h.isFramePerfect;
            break;
        }
    }

    if (!willDie) return result;

    // Phase 2: will we survive WITH jumping?
    const float jumpVy = vy + JUMP_VY;
    bool jumpSafe = true;

    for (const auto& h : m_hazards) {
        CCRect hr { h.x, h.y, h.width, h.height };
        if (trajectoryHits(playerPos.x, playerPos.y, vx, jumpVy, hr, STEPS, SIM_DT)) {
            jumpSafe = false;
            break;
        }
    }

    if (jumpSafe) {
        result.shouldJump     = true;
        result.framePerfect   = isFP;
        result.distanceToEdge = dangerX - playerPos.x;
    }
    return result;
}

// ── Hazard filter ─────────────────────────────────────────────────────────────
bool AIEngine::isHazard(GameObject* obj) {
    if (!obj) return false;
    const GameObjectType t = obj->m_objectType;
    // Whitelist only collidable/deadly types; skip decorations, triggers, BGs
    return (t == GameObjectType::Solid   ||
            t == GameObjectType::Hazard  ||
            t == GameObjectType::Spike);
}

// ── AABB trajectory simulation ────────────────────────────────────────────────
bool AIEngine::trajectoryHits(float px, float py,
                               float vx, float vy,
                               const CCRect& rect,
                               int steps, float /*dt*/)
{
    // dt param kept for future use; physics uses per-tick deltas directly.
    constexpr float HX = PLAYER_HALF;
    constexpr float HY = PLAYER_HALF;

    const float rL = rect.origin.x;
    const float rR = rL + rect.size.width;
    const float rB = rect.origin.y;
    const float rT = rB + rect.size.height;

    for (int i = 0; i < steps; ++i) {
        px += vx;
        vy += GRAVITY;
        py += vy;

        if ((px + HX) > rL && (px - HX) < rR &&
            (py + HY) > rB && (py - HY) < rT)
        {
            return true;
        }
    }
    return false;
}

