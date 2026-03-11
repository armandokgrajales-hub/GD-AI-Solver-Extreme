// ─────────────────────────────────────────────────────────────────────────────
//  hooks/PauseLayerHook.cpp  —  GD AI Solver Extreme
//
//  Injects an AI toggle button into the Pause menu.
//
//  UX behaviour
//  ────────────
//  • Checkmark (GJ_checkOn_001.png)  → AI is ON
//  • Red X      (edit_xBtn_001.png)  → AI is OFF
//  • State is persisted via Mod::get()->setSavedValue so it survives
//    app restarts and level switches.
//  • Disabling mid-level immediately stops the engine via AIEngine::setEnabled.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "../AIEngine.hpp"

using namespace geode::prelude;
using namespace GDAI;

// ─── PauseLayer hook ──────────────────────────────────────────────────────────
struct GDAISolverPauseLayer : Modify<GDAISolverPauseLayer, PauseLayer> {

    // ── customSetup is the Geode-recommended extension point for PauseLayer ──
    void customSetup() {
        PauseLayer::customSetup();

        // ── Read persisted state ─────────────────────────────────────────────
        const bool aiOn = Mod::get()->getSavedValue<bool>("ai-enabled", false);

        // ── Build toggle sprites ─────────────────────────────────────────────
        //  ON  sprite: green check  (GJ_checkOn_001.png  — built-in GD asset)
        //  OFF sprite: red X        (edit_xBtn_001.png   — built-in GD asset)
        auto* sprOn  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        auto* sprOff = CCSprite::createWithSpriteFrameName("edit_xBtn_001.png");

        if (!sprOn || !sprOff) {
            geode::log::warn("[GD-AI] Toggle sprites not found — skipping UI inject.");
            return;
        }

        // Scale to a tasteful size that fits the pause-menu button row
        sprOn ->setScale(0.7f);
        sprOff->setScale(0.7f);

        // ── Create the CCMenuItemToggler ─────────────────────────────────────
        //  CCMenuItemToggler shows sprOff when the underlying value is TRUE
        //  (i.e. "currently off, tap to turn on") — so we invert: pass !aiOn.
        //  The callback fires with the NEW state.
        auto* toggle = CCMenuItemToggler::create(
            sprOff,          // shown when AI is OFF  → tap to enable
            sprOn,           // shown when AI is ON   → tap to disable
            this,
            menu_selector(GDAISolverPauseLayer::onToggleAI)
        );

        if (!toggle) return;

        // CCMenuItemToggler::toggle(true) = show second sprite (ON state)
        toggle->toggle(!aiOn);   // invert because toggler semantics are inverted

        toggle->setTag(9901);    // arbitrary tag for future lookup

        // ── Position on screen ───────────────────────────────────────────────
        //  Place the button to the right of the normal pause-menu buttons.
        //  CCDirector gives us the actual screen size — safe for all resolutions.
        const CCSize winSize = CCDirector::get()->getWinSize();

        // Row of pause buttons sits at ~winSize.height * 0.55 on most layouts.
        // We nudge it to the right edge with a small margin.
        toggle->setPosition(winSize.width - 30.f, winSize.height * 0.55f);

        // ── Build a label so the player knows what the button does ───────────
        auto* label = CCLabelBMFont::create("AI", "bigFont.fnt");
        label->setScale(0.45f);
        label->setPosition(winSize.width - 30.f, winSize.height * 0.55f - 22.f);
        label->setColor({ 255, 220, 50 });  // yellow — stands out on dark BG

        // ── Add to the existing pause-menu ───────────────────────────────────
        //  getChildByID("main-menu") is the Geode-tagged main button menu.
        //  Fall back to a raw CCMenu if the tag isn't found.
        CCMenu* menu = nullptr;
        if (auto* node = getChildByID("main-menu")) {
            menu = dynamic_cast<CCMenu*>(node);
        }
        if (!menu) {
            // Fallback: create a detached menu anchored at (0,0)
            menu = CCMenu::create();
            menu->setPosition(CCPointZero);
            addChild(menu, 10);
        }

        menu->addChild(toggle);
        addChild(label, 10);    // labels sit outside the menu
    }

private:
    // ── Toggle callback ───────────────────────────────────────────────────────
    void onToggleAI(CCObject* sender) {
        auto* toggle = static_cast<CCMenuItemToggler*>(sender);
        if (!toggle) return;

        // isToggled() == true means ON sprite is now showing → AI enabled.
        // Geode's CCMenuItemToggler flips state BEFORE calling the callback.
        const bool nowEnabled = toggle->isToggled();

        // Persist
        Mod::get()->setSavedValue("ai-enabled", nowEnabled);

        // Apply immediately to the running engine
        AIEngine::get().setEnabled(nowEnabled);

        geode::log::info("[GD-AI] Toggled via Pause UI → enabled={}", nowEnabled);

        // ── Visual feedback: flash the label ─────────────────────────────────
        const CCSize winSize = CCDirector::get()->getWinSize();
        const char* msg = nowEnabled ? "AI: ON" : "AI: OFF";
        const ccColor3B col = nowEnabled
            ? ccColor3B{80, 255, 80}
            : ccColor3B{255, 80, 80};

        auto* flash = CCLabelBMFont::create(msg, "bigFont.fnt");
        flash->setScale(0.6f);
        flash->setColor(col);
        flash->setPosition(winSize.width / 2.f, winSize.height / 2.f + 60.f);
        flash->setOpacity(255);
        addChild(flash, 99);

        // Fade out the feedback label after 1.2 seconds
        flash->runAction(CCSequence::create(
            CCDelayTime::create(0.8f),
            CCFadeOut::create(0.4f),
            CCCallFuncN::create(this, callfuncN_selector(GDAISolverPauseLayer::removeFeedbackLabel)),
            nullptr
        ));
    }

    void removeFeedbackLabel(CCNode* label) {
        if (label) label->removeFromParent();
    }
};
