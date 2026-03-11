// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  —  GD AI Solver Extreme
//  Geode mod entry point.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// $on_mod values are lowercase in Geode 4.x: loaded / unloaded / enabled / disabled
$on_mod(loaded) {
    geode::log::info("[GD-AI] GD AI Solver Extreme v{} loaded. By armandokgrajales",
        Mod::get()->getVersion().toNonVString());
}

$on_mod(unloaded) {
    geode::log::info("[GD-AI] GD AI Solver Extreme unloaded.");
}

