// ─────────────────────────────────────────────────────────────────────────────
//  hooks/PauseLayerHook.cpp  —  GD AI Solver Extreme
//
//  Injects an AI on/off toggle into the Pause menu.
//
//  Toggle logic:
//   CCMenuItemToggler stores an internal bool m_toggled.
//   toggle(false) → shows first sprite (sprOff = red X)   → AI OFF
//   toggle(true)  → shows second sprite (sprOn = checkmark) → AI ON
//   isToggled()   → returns current m_toggled value
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "../AIEngine.hpp"

using namespace geode::prelude;
using namespace GDAI;

struct GDAISolverPauseLayer : Modify<GDAISolverPauseLayer, PauseLayer> {

    void customSetup() {
        PauseLayer::customSetup();
        buildToggleButton();
    }

private:

    void buildToggleButton() {
        const bool aiOn = Mod::get()->getSavedValue<bool>("ai-enabled", false);

        // Use built-in GD sprite frames — always present in 2.2
        auto* sprOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        auto* sprOn  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");

        if (!sprOff || !sprOn) {
            // Fallback to colored labels if sprites are unavailable
            geode::log::warn("[GD-AI] Sprite frames not found, using label fallback.");
            buildLabelButton(aiOn);
            return;
        }

        sprOff->setScale(0.75f);
        sprOn ->setScale(0.75f);

        // CCMenuItemToggler: first arg shown when NOT toggled (off), second when toggled (on)
        auto* toggle = CCMenuItemToggler::create(
            sprOff, sprOn,
            this,
            menu_selector(GDAISolverPauseLayer::onToggleAI)
        );
        if (!toggle) return;

        // Sync visual state with saved value
        // toggle(true) = show "on" sprite = AI enabled
        toggle->toggle(aiOn);
        toggle->setTag(9901);

        positionAndAdd(toggle);

        // "AI" label below the button
        auto* lbl = CCLabelBMFont::create("AI", "bigFont.fnt");
        if (lbl) {
            lbl->setScale(0.40f);
            lbl->setColor({ 255, 220, 50 });
            const CCSize ws = CCDirector::get()->getWinSize();
            lbl->setPosition(ws.width - 28.f, ws.height * 0.55f - 22.f);
            addChild(lbl, 10);
        }
    }

    // Positions the toggle inside the existing pause menu
    void positionAndAdd(CCMenuItemToggler* toggle) {
        const CCSize ws = CCDirector::get()->getWinSize();
        toggle->setPosition(ws.width - 28.f, ws.height * 0.55f);

        CCMenu* menu = nullptr;
        if (auto* node = getChildByID("main-menu"))
            menu = dynamic_cast<CCMenu*>(node);

        if (!menu) {
            menu = CCMenu::create();
            menu->setPosition(CCPointZero);
            addChild(menu, 10);
        }
        menu->addChild(toggle);
    }

    // Simple colored-label fallback (no sprite dependency)
    void buildLabelButton(bool aiOn) {
        const CCSize ws = CCDirector::get()->getWinSize();
        const char* txt = aiOn ? "AI:ON" : "AI:OFF";

        auto* item = CCMenuItemLabel::create(
            CCLabelBMFont::create(txt, "bigFont.fnt"),
            this,
            menu_selector(GDAISolverPauseLayer::onToggleFallback)
        );
        if (!item) return;
        item->setScale(0.45f);
        item->setTag(9902);

        auto* menu = CCMenu::create();
        menu->setPosition(CCPointZero);
        addChild(menu, 10);
        menu->addChild(item);
        item->setPosition(ws.width - 30.f, ws.height * 0.55f);
    }

    // ── Main toggle callback ──────────────────────────────────────────────────
    void onToggleAI(CCObject* sender) {
        auto* toggle = static_cast<CCMenuItemToggler*>(sender);
        if (!toggle) return;

        // isToggled() returns the NEW state after the click flips it
        const bool nowEnabled = toggle->isToggled();

        Mod::get()->setSavedValue("ai-enabled", nowEnabled);
        AIEngine::get().setEnabled(nowEnabled);

        geode::log::info("[GD-AI] Toggle → enabled={}", nowEnabled);
        showFeedback(nowEnabled);
    }

    // ── Fallback (label-only) toggle callback ─────────────────────────────────
    void onToggleFallback(CCObject* /*sender*/) {
        const bool nowEnabled = !AIEngine::get().isEnabled();
        Mod::get()->setSavedValue("ai-enabled", nowEnabled);
        AIEngine::get().setEnabled(nowEnabled);

        // Update label text
        if (auto* item = dynamic_cast<CCMenuItemLabel*>(getChildByTag(9902))) {
            if (auto* lbl = dynamic_cast<CCLabelBMFont*>(item->getLabel()))
                lbl->setString(nowEnabled ? "AI:ON" : "AI:OFF");
        }
        showFeedback(nowEnabled);
    }

    // ── Brief on-screen feedback flash ───────────────────────────────────────
    void showFeedback(bool enabled) {
        const CCSize ws  = CCDirector::get()->getWinSize();
        const char*  msg = enabled ? "AI: ON" : "AI: OFF";
        const ccColor3B col = enabled
            ? ccColor3B{ 80, 255, 80 }
            : ccColor3B{ 255, 80, 80 };

        auto* lbl = CCLabelBMFont::create(msg, "bigFont.fnt");
        if (!lbl) return;
        lbl->setScale(0.6f);
        lbl->setColor(col);
        lbl->setPosition(ws.width / 2.f, ws.height / 2.f + 60.f);
        addChild(lbl, 99);

        lbl->runAction(CCSequence::create(
            CCDelayTime::create(0.7f),
            CCFadeOut::create(0.3f),
            CCRemoveSelf::create(),
            nullptr
        ));
    }
};

