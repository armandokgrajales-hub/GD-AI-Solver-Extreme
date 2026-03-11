// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  —  GD AI Solver Extreme
//  Geode mod entry point.
//
//  NOTE: In Geode 4.10.2 the only valid $on_mod value is `Loaded` (capital L).
//  `Unloaded`, `Enabled`, `Disabled` do NOT exist in this SDK version.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>

using namespace geode::prelude;

$on_mod(Loaded) {
    geode::log::info("[GD-AI] GD AI Solver Extreme v{} loaded. By armandokgrajales",
        Mod::get()->getVersion().toNonVString());
}

