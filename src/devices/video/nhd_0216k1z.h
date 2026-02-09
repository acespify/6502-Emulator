#pragma once
#include "../../emu/types.h"
#include <vector>
#include <string>

// ============================================================================
//  Device: NHD-0216K1Z (2x16 LCD)
// ============================================================================
//  Updated for 4-Bit Interface Support (ST7066U)
// ============================================================================
class nhd_0216k1z {
public:
    nhd_0216k1z();

    // --- 4-Bit Hardware Connection (Schematic U3 <-> U5) ---
    // In 4-bit mode, we receive data on DB4-DB7 (upper nibble of byte).
    // The RS/RW/E lines are separate control bits.
    void write_4bit(u8 nibble_data, bool rs, bool rw, bool e);

    // Visual Output
    const std::vector<std::string>& get_display_lines() { return m_display; }

private:
    u8 m_ddram[80];
    std::vector<std::string> m_display;

    // --- 4-Bit State Machine ---
    bool m_nibble_flip = false; // False = Waiting for High Nibble, True = Waiting for Low
    u8   m_high_nibble_latch = 0;
    bool m_prev_e = false;      // To detect Falling Edge

    u8   m_cursor_pos = 0;
    
    // Commands
    void process_byte(u8 byte, bool rs);
    void update_visuals();
};