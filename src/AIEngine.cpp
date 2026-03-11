// ─────────────────────────────────────────────────────────────────────────────
//  AIEngine.cpp  —  GD AI Solver Extreme
//  Core analysis + decision logic.
// ─────────────────────────────────────────────────────────────────────────────
#include "AIEngine.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;
using namespace GDAI;

// ─── Geometry Dash constants (approximate, tuned for 2.2) ────────────────────
//  GD runs at 240 physics ticks/sec internally but the visual FPS is 60.
//  On mobile, dt can exceed 1/60 s under load, so we use real dt everywhere.
static constexpr float PLAYER_HALF_SIZE   = 10.f;  // half-width of player hitbox (px)
static constexpr float PLAYER_HEIGHT      = 20.f;  // full height of player hitbox
static constexpr float JUMP_VELOCITY_Y    = 11.18f; // pixels/tick (GD cube base jump)
static constexpr float GRAVITY            = -0.958f; // pixels/tick² (approx)
static constexpr float PLAYER_SPEED_X     = 8.37f;  // pixels/tick (speed x1)
static constexpr float TICKS_PER_SECOND   = 240.f;

// ─── Session lifecycle ────────────────────────────────────────────────────────
void AIEngine::onSessionStart() {
    // Re-read saved settings so a mid-game change takes effect next attempt.
    auto* mod = Mod::get();
    m_lookahead          = mod->getSavedValue<float>("lookahead-distance",     320.f);
    m_framePerfectThresh = mod->getSavedValue<float>("frame-perfect-threshold", 22.f);
    m_humanVarianceMs    = mod->getSavedValue<int>  ("human-variance-ms",       15  );
    m_enabled            = mod->getSavedValue<bool> ("ai-enabled",             false);

    m_hazards.clear();
    m_humanTimer.cancel();
    m_frameCounter   = 0;
    m_jumpScheduled  = false;
    m_holdingJump    = false;
    m_sessionActive  = true;

    geode::log::info("[GD-AI] Session started | enabled={} lookahead={}px fpThresh={}px variance={}ms",
        m_enabled.load(), m_lookahead, m_framePerfectThresh, m_humanVarianceMs);
}

void AIEngine::onSessionEnd() {
    m_sessionActive = false;
    m_hazards.clear();
    m_humanTimer.cancel();
    m_holdingJump   = false;
    m_jumpScheduled = false;
}

// ─── Per-frame update ─────────────────────────────────────────────────────────
void AIEngine::update(PlayLayer* layer, float dt) {
    // ── Guard rails ─────────────────────────────────────────────────────────
    if (!m_enabled || !m_sessionActive) return;

    // Null-safety: on mobile, layer / player can be in a transitional state.
    if (!layer) return;

    PlayerObject* player = layer->m_player1;
    if (!player) return;

    // ── Cache player world position ─────────────────────────────────────────
    const CCPoint playerPos = player->getPosition();

    // ── Periodic hazard scan (not every frame — saves CPU on mobile) ────────
    ++m_frameCounter;
    if (m_frameCounter >= SCAN_INTERVAL_FRAMES) {
        m_frameCounter = 0;
        scanHazards(layer, playerPos);
    }

    // ── Tick the human-delay timer ───────────────────────────────────────────
    if (m_jumpScheduled && m_humanTimer.tick(dt)) {
        // Delay elapsed — actually press jump
        m_jumpScheduled = false;
        m_holdingJump   = true;
        player->pushButton(PlayerButton::Jump);
    }

    // ── Release jump after one full frame (simulates tap, not hold) ─────────
    if (m_holdingJump) {
        // Release on the very next tick after pressing
        m_holdingJump = false;
        player->releaseButton(PlayerButton::Jump);
        return; // do not re-evaluate this frame
    }

    // ── If a click is already pending (in delay), don't queue another ───────
    if (m_jumpScheduled || m_humanTimer.isCounting()) return;

    // ── Compute jump decision ────────────────────────────────────────────────
    JumpDecision decision = computeDecision(player, playerPos);

    if (decision.shouldJump) {
        // Frame-perfect section → zero variance
        int variance = decision.framePerfect ? 0 : m_humanVarianceMs;
        m_humanTimer.scheduleClick(variance);
        m_jumpScheduled = true;

        geode::log::debug("[GD-AI] Jump scheduled | fp={} dist={:.1f}px delay={}ms",
            decision.framePerfect, decision.distanceToEdge, variance);
    }
}

// ─── Hazard scan ─────────────────────────────────────────────────────────────
void AIEngine::scanHazards(PlayLayer* layer, const CCPoint& playerPos) {
    m_hazards.clear();

    // m_objects is the full object list; we slice by x-range for performance.
    CCArray* objects = layer->m_objects;
    if (!objects) return;

    const float leftBound  = playerPos.x - 20.f;          // small buffer behind
    const float rightBound = playerPos.x + m_lookahead;   // look-ahead window

    // Reserve to avoid repeated allocations on mobile heap
    m_hazards.reserve(32);

    for (int i = 0; i < static_cast<int>(objects->count()); ++i) {
        auto* obj = static_cast<GameObject*>(objects->objectAtIndex(i));
        if (!obj) continue;
        if (!isHazard(obj)) continue;

        const CCRect rect = obj->getObjectRect();

        // Coarse X cull — only objects overlapping the look-ahead strip
        if (rect.getMaxX() < leftBound || rect.getMinX() > rightBound) continue;

        HazardInfo info;
        info.x     = rect.origin.x;
        info.y     = rect.origin.y;
        info.width = rect.size.width;
        info.height= rect.size.height;

        float distToEdge       = rect.origin.x - playerPos.x;
        info.isFramePerfect    = (distToEdge <= m_framePerfectThresh);

        m_hazards.push_back(info);
    }
}

// ─── Decision logic ──────────────────────────────────────────────────────────
JumpDecision AIEngine::computeDecision(PlayerObject* player,
                                        const CCPoint& playerPos)
{
    JumpDecision result;
    if (m_hazards.empty()) return result;

    // Current velocities (GD exposes these through m_yVelocity, m_xVelocity)
    const float vx = player->m_xVelocity;
    float       vy = player->m_yVelocity;

    // Simulation parameters
    // We run 60 look-ahead ticks (~0.25 s of game time at 240 ticks/s)
    constexpr int   SIM_STEPS   = 60;
    constexpr float SIM_DT      = 1.f / TICKS_PER_SECOND;

    // Player hitbox half-extents
    const CCRect playerRect = player->getObjectRect();
    const float  pw         = playerRect.size.width;
    const float  ph         = playerRect.size.height;

    // ── Phase 1: will we die WITHOUT jumping? ────────────────────────────────
    bool willDie = false;
    float dangerX = 0.f;

    for (const auto& h : m_hazards) {
        CCRect hazardRect = { h.x, h.y, h.width, h.height };
        if (trajectoryHits(playerPos.x, playerPos.y, vx, vy,
                           hazardRect, SIM_STEPS, SIM_DT))
        {
            willDie  = true;
            dangerX  = h.x;
            result.framePerfect = h.isFramePerfect;
            break; // first hazard is usually the closest — good enough
        }
    }

    if (!willDie) return result; // safe — no jump needed

    // ── Phase 2: will we survive WITH jumping? ───────────────────────────────
    float jumpVy = vy + JUMP_VELOCITY_Y; // apply instant jump impulse

    bool jumpSafe = true;
    for (const auto& h : m_hazards) {
        CCRect hazardRect = { h.x, h.y, h.width, h.height };
        if (trajectoryHits(playerPos.x, playerPos.y, vx, jumpVy,
                           hazardRect, SIM_STEPS, SIM_DT))
        {
            jumpSafe = false;
            break;
        }
    }

    if (jumpSafe) {
        result.shouldJump     = true;
        result.distanceToEdge = dangerX - playerPos.x;
    }
    // If jumping is ALSO deadly, the AI refrains (level may require a double-tap
    // or a different action not yet implemented — returns shouldJump=false).
    return result;
}

// ─── Hazard classification ────────────────────────────────────────────────────
bool AIEngine::isHazard(GameObject* obj) {
    if (!obj) return false;

    // GD object types relevant to collision:
    //   kGameObjectTypeSpike     (spikes)    → always deadly
    //   kGameObjectTypeSolid     (blocks)    → solid, can land on OR die hitting
    //   kGameObjectTypeHazard    (saws, pads)→ depends on subtype
    //   Decoration types: kGameObjectTypeDecoration, kGameObjectTypeSpecial
    //     → safe to IGNORE (they have no hitbox in game logic)

    // We whitelist rather than blacklist to be safe on new GD object types.
    const GameObjectType t = obj->m_objectType;
    return (t == GameObjectType::Solid      ||
            t == GameObjectType::Hazard     ||
            t == GameObjectType::Spike);
    // Note: Portals, pads, orbs have their own type — intentionally excluded here
    // to avoid jumping at speed portals etc.  Extend as needed.
}

// ─── Trajectory simulation ────────────────────────────────────────────────────
bool AIEngine::trajectoryHits(float px, float py,
                               float vx, float vy,
                               const CCRect& rect,
                               int steps, float dt)
{
    // Player half-extents (approximate — adjust to match game hitbox)
    constexpr float HX = PLAYER_HALF_SIZE;
    constexpr float HY = PLAYER_HALF_SIZE;

    for (int i = 0; i < steps; ++i) {
        px += vx;
        vy += GRAVITY;   // apply gravity each tick
        py += vy;

        // AABB overlap test: player box vs hazard rect
        // Player box: [px - HX, px + HX] × [py - HY, py + HY]
        const float pLeft   = px - HX;
        const float pRight  = px + HX;
        const float pBottom = py - HY;
        const float pTop    = py + HY;

        const float rLeft   = rect.origin.x;
        const float rRight  = rLeft + rect.size.width;
        const float rBottom = rect.origin.y;
        const float rTop    = rBottom + rect.size.height;

        if (pRight  > rLeft  &&
            pLeft   < rRight &&
            pTop    > rBottom&&
            pBottom < rTop)
        {
            return true; // collision detected
        }
    }
    return false;
}
