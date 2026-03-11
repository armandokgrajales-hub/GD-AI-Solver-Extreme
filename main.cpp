// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  —  GD AI Solver Extreme
//
//  Geode entry point.
//  All hook registration happens automatically via the $modify() macros in
//  PlayLayerHook.cpp and PauseLayerHook.cpp — nothing extra is needed here.
//
//  This file exists as the translation unit that Geode looks for as the
//  DLL/SO entry point.
// ─────────────────────────────────────────────────────────────────────────────
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// Geode's own macro marks the mod entry point and handles __attribute__
// visibility / DllMain on every platform.
$on_mod(Loaded) {
    geode::log::info("GD AI Solver Extreme v{} loaded. Developer: armandokgrajales",
        Mod::get()->getVersion().toNonVString());
}

$on_mod(Unloaded) {
    geode::log::info("GD AI Solver Extreme unloaded.");
}
