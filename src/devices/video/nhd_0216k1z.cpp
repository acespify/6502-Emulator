#include "nhd_0216k1z.h"
#include <algorithm>

#include "nhd_0216k1z.h"
#include <algorithm>

nhd_0216k1z::nhd_0216k1z() {
    std::fill(std::begin(m_ddram), std::end(m_ddram), 0x20); // Clear with spaces
    m_display.resize(2, std::string(16, ' '));
}

// ============================================================================
//  Write 4-Bit (Simulates VIA Port B updates)
// ============================================================================
void nhd_0216k1z::write_4bit(u8 nibble_data, bool rs, bool rw, bool e) {
    // 1. Detect Falling Edge of E (Enable)
    if (m_prev_e && !e) {
        
        // Data is on DB4-DB7. We mask it just in case.
        u8 data_on_bus = nibble_data & 0xF0;

        // SPECIAL CASE: Initialization Sequence (0x33, 0x32)
        // The standard 4-bit init sequence sends single nibbles that act as commands.
        // We handle this by checking if we are sending "Function Set" commands 
        // that force 4-bit mode.
        // For emulation simplicity: We assume standard 2-cycle transfer UNLESS
        // we detect the specific "Switch to 4-bit" command (0x20 nibble).
        
        if (!rs && !m_nibble_flip && (data_on_bus == 0x20 || data_on_bus == 0x30)) {
            // This is likely the single-nibble init commands. 
            // 0x30 = Reset/Wakeup (sent 3 times)
            // 0x20 = "Switch to 4-bit mode"
            // We treat these as single-shot executions.
             // (Do nothing internally, just reset flip-flop to ensure we start fresh)
             m_nibble_flip = false; 
             // Note: Real ST7066U is complex here, but this suffices for WozMon/Ben's code.
             return; 
        }

        if (!m_nibble_flip) {
            // STEP 1: Store High Nibble
            m_high_nibble_latch = data_on_bus;
            m_nibble_flip = true; // Expect Low Nibble next
        } 
        else {
            // STEP 2: Combine with Low Nibble
            // The data_on_bus IS the low nibble, but physically on lines DB4-DB7.
            // So we shift it down.
            u8 full_byte = m_high_nibble_latch | (data_on_bus >> 4);
            
            process_byte(full_byte, rs);
            
            m_nibble_flip = false; // Reset to wait for next command
        }
    }
    m_prev_e = e;
}

void nhd_0216k1z::process_byte(u8 byte, bool rs) {
    if (!rs) {
        // --- COMMAND MODE ---
        if (byte & 0x80) { m_cursor_pos = byte & 0x7F; } // Set DDRAM Addr
        else if (byte == 0x01) {                         // Clear Display
            std::fill(std::begin(m_ddram), std::end(m_ddram), 0x20);
            m_cursor_pos = 0;
        }
        else if (byte == 0x02) { m_cursor_pos = 0; }     // Return Home
    } 
    else {
        // --- DATA MODE ---
        if (m_cursor_pos < 80) {
            m_ddram[m_cursor_pos] = byte;
            m_cursor_pos++;
        }
    }
    update_visuals();
}

void nhd_0216k1z::update_visuals() {
    for (int i = 0; i < 16; i++) {
        m_display[0][i] = (char)m_ddram[0x00 + i];
        m_display[1][i] = (char)m_ddram[0x40 + i];
    }
}