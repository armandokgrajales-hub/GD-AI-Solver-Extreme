// ─────────────────────────────────────────────────────────────────────────────
//  hooks/PauseLayerHook.cpp  —  GD AI Solver Extreme
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "../AIEngine.hpp"

using namespace geode::prelude;
using namespace GDAI;

struct GDAISolverPauseLayer : Modify<GDAISolverPauseLayer, PauseLayer> {

    void customSetup() {
        PauseLayer::customSetup();

        const bool aiOn = Mod::get()->getSavedValue<bool>("ai-enabled", false);

        // Use GJ_checkOff / GJ_checkOn — present in all GD 2.2 builds
        auto* sprOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        auto* sprOn  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        if (!sprOff || !sprOn) return; // sprites missing — skip silently

        sprOff->setScale(0.75f);
        sprOn ->setScale(0.75f);

        // First arg = shown when NOT toggled (AI off), second = shown when toggled (AI on)
        auto* toggle = CCMenuItemToggler::create(
            sprOff, sprOn,
            this,
            menu_selector(GDAISolverPauseLayer::onToggleAI)
        );
        if (!toggle) return;

        toggle->toggle(aiOn); // sync to saved state
        toggle->setTag(9901);

        const CCSize ws = CCDirector::get()->getWinSize();
        toggle->setPosition(ws.width - 28.f, ws.height * 0.55f);

        // "AI" label below the button
        auto* lbl = CCLabelBMFont::create("AI", "bigFont.fnt");
        if (lbl) {
            lbl->setScale(0.40f);
            lbl->setColor({ 255, 220, 50 });
            lbl->setPosition(ws.width - 28.f, ws.height * 0.55f - 22.f);
            addChild(lbl, 10);
        }

        // Attach to the existing pause menu if possible, else a new one
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

private:
    void onToggleAI(CCObject* sender) {
        auto* toggle = static_cast<CCMenuItemToggler*>(sender);
        if (!toggle) return;

        const bool nowEnabled = toggle->isToggled();
        Mod::get()->setSavedValue("ai-enabled", nowEnabled);
        AIEngine::get().setEnabled(nowEnabled);
        geode::log::info("[GD-AI] Toggle → enabled={}", nowEnabled);

        // Brief on-screen feedback
        const CCSize ws  = CCDirector::get()->getWinSize();
        auto* flash = CCLabelBMFont::create(nowEnabled ? "AI:ON" : "AI:OFF", "bigFont.fnt");
        if (!flash) return;
        flash->setScale(0.55f);
        flash->setColor(nowEnabled ? ccColor3B{80,255,80} : ccColor3B{255,80,80});
        flash->setPosition(ws.width / 2.f, ws.height / 2.f + 60.f);
        addChild(flash, 99);
        flash->runAction(CCSequence::create(
            CCDelayTime::create(0.7f),
            CCFadeOut::create(0.3f),
            CCRemoveSelf::create(),
            nullptr
        ));
    }
};

