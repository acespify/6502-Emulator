// ============================================================================
// Copyright (c) 2026 Andrew Young
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
// 
// ============================================================================

#include "config.h"
#include <fstream>
#include <iostream>
#include "../vendor/json/json.hpp"

using json = nlohmann::json;

void EmulatorConfig::apply_schematic_rules() {
    // Auto-close windows that don't belong in the current schematic
    if (machine_type == 0) { // SCHEMATIC_1_BASIC
        show_acia = false;
    } else if ( machine_type == 1) {
        show_acia = true;
    }
}

void EmulatorConfig::load(const std::string& filename){
    std::ifstream file(filename);
    if (!file.is_open()){
        std::cout << "[Config] No existing config found. Creating Default." << std::endl;
        apply_schematic_rules();
        return;
    }

    try {
        json j;
        file >> j;

        // .value(key, default_value) safely falls back if the key is missing
        machine_type = j.value("machine_type", 1);
        show_cpu     = j.value("show_cpu", true);
        show_stack   = j.value("show_stack", true);
        show_via     = j.value("show_via", false);
        show_acia    = j.value("show_acia", false);
        show_ram     = j.value("show_ram", true);
        show_rom     = j.value("show_rom", false);
        show_lcd     = j.value("show_lcd", true);
        show_speed   = j.value("show_speed", false);
        show_log     = j.value("show_log", true);

        // Ensure we don't load an invalid state (e.g. ACIA open on Basic schematic)
        apply_schematic_rules();

    } catch (const std::exception& e) {
        std::cerr << "[Config] Error parsing JSON: " << e.what() << std::endl;
    }

}

void EmulatorConfig::save(const std::string& filename) const {
    json j;
    j["machine_type"] = machine_type;
    j["show_cpu"]     = show_cpu;
    j["show_stack"]   = show_stack;
    j["show_via"]     = show_via;
    j["show_acia"]    = show_acia;
    j["show_ram"]     = show_ram;
    j["show_rom"]     = show_rom;
    j["show_lcd"]     = show_lcd;
    j["show_speed"]   = show_speed;
    j["show_log"]     = show_log;

    std::ofstream file(filename);
    if (file.is_open()) {
        // The '4' adds pretty-printing with 4 spaces of indentation
        file << j.dump(4) << std::endl; 
    }
}