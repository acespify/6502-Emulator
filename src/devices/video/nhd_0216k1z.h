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
    void write_4bit(u8 data_lines, bool rs, bool rw, bool e);

    void write_8bit(u8 byte, bool rs, bool rw);

    // Visual Output
    const std::vector<std::string>& get_display_lines();

    u8   get_cursor_addr() const { return m_ac; }
    bool is_cursor_on()    const { return m_cursor_on; }
    bool is_blink_on()     const { return m_blink_on; }
    bool is_display_on()   const { return m_display_on; }
    bool is_8bit_mode()    const { return m_8bit_mode; }

private:
    // Internal Memory
    u8 m_ddram[0x80];       // Display Data RAM
    u8 m_cgram[0x40];       // Character Generator RAM (Custom Chars)
    
    // Internal Registers
    u8 m_ac = 0;            
    
    // Configuration Flags
    bool m_8bit_mode = true;
    bool m_display_on = false;
    bool m_cursor_on = false;
    bool m_blink_on = false;
    bool m_increment = true;
    bool m_shift = false;
    
    // 4-Bit State
    bool m_nibble_flip = false; 
    u8   m_high_nibble = 0;
    bool m_prev_e = false;      

    // --- Helpers ---
    void process_instruction(u8 cmd);
    void write_data(u8 data);
    void update_visuals(); 
    
    // [NEW] Maps LCD Byte -> UTF-8 String based on Table 4
    std::string map_rom_code_to_utf8(u8 lcd_byte);

    // Visual Buffer
    std::vector<std::string> m_display_cache;
};