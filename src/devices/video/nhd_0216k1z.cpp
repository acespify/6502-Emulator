#include "nhd_0216k1z.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

// ============================================================================
//  CONSTRUCTOR
// ============================================================================
nhd_0216k1z::nhd_0216k1z() {
    // 1. Initialize Memory
    // Fill DDRAM with spaces (0x20)
    std::memset(m_ddram, 0x20, sizeof(m_ddram));
    // Clear CGRAM
    std::memset(m_cgram, 0x00, sizeof(m_cgram));
    
    // 2. Initialize Display Cache
    // We use a vector of strings to hold the UTF-8 renderable text
    m_display_cache.resize(2);
    m_display_cache[0] = "                "; // 16 spaces
    m_display_cache[1] = "                ";

    // 3. Defaults
    m_8bit_mode = true; // LCDs power up in 8-bit mode
    m_display_on = false;
    m_cursor_on = false;
    m_blink_on = false;
    m_increment = true;
    m_shift = false;
}

// ============================================================================
//  4-BIT INTERFACE (Matches Schematic)
// ============================================================================
void nhd_0216k1z::write_4bit(u8 data_lines, bool rs, bool rw, bool e) {
    // 1. Detect Falling Edge of E (High -> Low)
    if (m_prev_e && !e) {
        // Read the high nibble from the data lines (Bits 4-7)
        u8 nibble = data_lines & 0xF0; 

        if (m_8bit_mode) {
            // --- SPECIAL CASE: INITIALIZATION ---
            // When in 8-bit mode (power on), if we are wired for 4-bit,
            // we receive commands as single nibbles on the upper pins.
            
            // Function Set: Change to 4-bit mode (0x20)
            if (!rs && !rw && (nibble == 0x20)) {
                m_8bit_mode = false;
                m_nibble_flip = false; // Reset nibble tracker
            }
            // Function Set: 8-bit mode (0x30) - Used during "Wake Up"
            else if (!rs && !rw && (nibble == 0x30)) {
                // Do nothing, just stay in 8-bit mode. 
                // This is part of the "Wait... Reset... Wait" sequence.
                m_8bit_mode = true;
                m_nibble_flip = false;
            }
        }
        else {
            // --- NORMAL 4-BIT OPERATION ---
            if (!m_nibble_flip) {
                // First Nibble (High)
                m_high_nibble = nibble;
                m_nibble_flip = true;
            } 
            else {
                // Second Nibble (Low)
                // The data is on lines 7-4, so we shift it down to 3-0
                u8 low_nibble = (nibble >> 4);
                u8 full_byte = m_high_nibble | low_nibble;
                
                // Execute
                if (rs) write_data(full_byte);
                else    process_instruction(full_byte);
                
                m_nibble_flip = false; // Reset for next command
            }
        }
    }
    m_prev_e = e;
}

// ============================================================================
//  8-BIT INTERFACE
// ============================================================================
void nhd_0216k1z::write_8bit(u8 byte, bool rs, bool rw) {
    // Direct execution, no nibble assembly needed
    if (rs) write_data(byte);
    else    process_instruction(byte);
}

// ============================================================================
//  INSTRUCTION PROCESSOR (HD44780 Standard)
// ============================================================================
void nhd_0216k1z::process_instruction(u8 cmd) {
    
    // 1. CLEAR DISPLAY (0x01)
    if (cmd == 0x01) {
        std::memset(m_ddram, 0x20, sizeof(m_ddram));
        m_ac = 0;
        m_increment = true; 
    }
    
    // 2. RETURN HOME (0x02 - 0x03)
    else if ((cmd & 0xFE) == 0x02) {
        m_ac = 0;
    }
    
    // 3. ENTRY MODE SET (0x04 - 0x07)
    // Bit 1: I/D (Increment/Decrement)
    // Bit 0: S   (Shift)
    else if ((cmd & 0xFC) == 0x04) {
        m_increment = (cmd & 0x02);
        m_shift     = (cmd & 0x01);
    }
    
    // 4. DISPLAY CONTROL (0x08 - 0x0F)
    // Bit 2: D (Display On)
    // Bit 1: C (Cursor On)
    // Bit 0: B (Blink On)
    else if ((cmd & 0xF8) == 0x08) {
        m_display_on = (cmd & 0x04);
        m_cursor_on  = (cmd & 0x02);
        m_blink_on   = (cmd & 0x01);
    }
    
    // 5. CURSOR / DISPLAY SHIFT (0x10 - 0x1F)
    else if ((cmd & 0xF0) == 0x10) {
        bool sc = (cmd & 0x08); // Screen vs Cursor
        bool rl = (cmd & 0x04); // Right vs Left
        if (!sc) {
            // Move Cursor
            if (rl) m_ac++; else m_ac--;
        }
    }
    
    // 6. FUNCTION SET (0x20 - 0x3F)
    else if ((cmd & 0xE0) == 0x20) {
        m_8bit_mode = (cmd & 0x10);
        // We track lines/font but don't strictly enforce rendering limits here
    }
    
    // 7. SET CGRAM ADDRESS (0x40 - 0x7F)
    else if ((cmd & 0xC0) == 0x40) {
        m_ac = (cmd & 0x3F); 
    }
    
    // 8. SET DDRAM ADDRESS (0x80 - 0xFF)
    else if (cmd & 0x80) {
        m_ac = (cmd & 0x7F);
    }
    
    update_visuals();
}

// ============================================================================
//  DATA WRITE
// ============================================================================
void nhd_0216k1z::write_data(u8 data) {
    // Safety Wrap (80 bytes of RAM)
    if (m_ac >= 0x80) m_ac = 0; 
    
    m_ddram[m_ac] = data;
    
    // Increment / Decrement Cursor
    if (m_increment) m_ac++;
    else             m_ac--;
    
    update_visuals();
}

// ============================================================================
//  VISUAL UPDATE (Translation Layer)
// ============================================================================
void nhd_0216k1z::update_visuals() {
    // Rebuild the strings entirely because UTF-8 chars vary in byte length.
    
    // --- Line 1 (0x00 - 0x0F) ---
    m_display_cache[0].clear();
    for (int i = 0; i < 16; i++) {
        m_display_cache[0] += map_rom_code_to_utf8(m_ddram[0x00 + i]);
    }
    
    // --- Line 2 (0x40 - 0x4F) ---
    m_display_cache[1].clear();
    for (int i = 0; i < 16; i++) {
        m_display_cache[1] += map_rom_code_to_utf8(m_ddram[0x40 + i]);
    }
}

// ============================================================================
//  ROM CODE A00 MAPPING (From Table 4)
// ============================================================================
std::string nhd_0216k1z::map_rom_code_to_utf8(u8 b) {
    // 1. Standard ASCII (0x20 - 0x7D)
    // Note: 0x5C is Yen (¥) in A00 ROM, but Backslash (\) in ASCII.
    // We will use Yen to match the table provided.
    if (b >= 0x20 && b <= 0x7D) {
        if (b == 0x5C) return "¥"; 
        return std::string(1, (char)b);
    }

    // 2. Specific Symbols from Table 4
    switch (b) {
        // --- Special Symbols ---
        case 0x7E: return "→"; // Right Arrow
        case 0x7F: return "←"; // Left Arrow
        
        // --- Upper Symbols (Katakana / Greek / Math) ---
        case 0xA0: return " "; // Nbsp
        case 0xA1: return "｡";
        case 0xA2: return "｢";
        case 0xA3: return "｣";
        case 0xA4: return "､";
        case 0xA5: return "･";
        
        // Selected Greek/Math from bottom right of table
        case 0xDF: return "°"; // Degree
        case 0xE0: return "α"; // Alpha
        case 0xE2: return "β"; // Beta
        case 0xE3: return "ε"; // Epsilon
        case 0xE4: return "μ"; // Mu
        case 0xE5: return "σ"; // Sigma
        case 0xF2: return "θ"; // Theta
        case 0xF3: return "∞"; // Infinity
        case 0xF4: return "Ω"; // Omega
        case 0xF6: return "Σ"; // Sigma
        case 0xF7: return "π"; // Pi
        
        case 0xFD: return "÷"; // Divide
        case 0xFF: return "█"; // Black Block

        // 3. CGRAM (0x00 - 0x07) placeholder
        case 0x00: case 0x08: return " "; 
        
        default: return "."; // Unmapped characters show as dot
    }
}

const std::vector<std::string>& nhd_0216k1z::get_display_lines() {
    return m_display_cache;
}