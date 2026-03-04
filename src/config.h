// ============================================================================
// Copyright (c) 2026 Andrew Young
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
// 
// ============================================================================

#pragma once
#include <string>

struct EmulatorConfig {
    // Hardware State
    int machine_type = 1; // 0 = Basic, 1 = Serial

    // UI Window States
    bool show_cpu = true;
    bool show_stack = true;
    bool show_via = false;
    bool show_acia = false;
    bool show_ram = true;
    bool show_rom = false;
    bool show_lcd = true;
    bool show_speed = false;
    bool show_log = true;

    // Methods
    void load(const std::string& filename); // To load an existing format
    void save(const std::string& filename) const; // Save current format
    void apply_schematic_rules();           // Apply rules for Schematic preference
};