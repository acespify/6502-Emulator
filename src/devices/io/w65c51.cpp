// ============================================================================
// Copyright (c) 2026 Andrew Young
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
// 
// ============================================================================

#include "w65c51.h"
#include "../../emu/map.h"

// Register Offsets
enum { DATA = 0, STATUS = 1, COMMAND = 2, CONTROL = 3 };

w65c51::w65c51() {
    reset();
}

void w65c51::memory_map(address_map& map) {
    map.install(0x0000, 0x0003, // Typically repeats every 4 bytes
        [this](u16 addr) { return this->read(addr); },
        [this](u16 addr, u8 data) { this->write(addr, data); }
    );
}

void w65c51::reset() {
    m_command_reg   = 0x00;
    m_control_reg   = 0x00;
    m_status_reg    = 0x10;
    m_rx_buffer     = 0x00;

    // Need to clear out any stale transmitted bytes
    while(!m_tx_buffer.empty()) m_tx_buffer.pop();

    update_irq();
}

// ============================================================================
// ACIA READ LOGIC
// ============================================================================
u8 w65c51::read(u16 addr) {
    switch (addr & 0x03) {
        case DATA:
            // Reading Data clears Rx Full flag
            m_status_reg &= ~0x08; // Clear Bit 3 (Rx Full) - W65C51 specific bit pos
            // Note: Older 6551 used Bit 3 for Rx Full, W65C51 might vary. 
            // Standard 6551: Bit 3 = Rx Full, Bit 4 = Tx Empty.
            m_status_reg &= ~0x80;
            update_irq();
            return m_rx_buffer;
            break;
        case STATUS:
            // Reading Status clears IRQ bit (Bit 7) on some versions
            {
                u8 res = m_status_reg;
                m_status_reg &= ~0x80; // Clear IRQ flag
                update_irq();
                return res;
            }
            break;
        case COMMAND: return m_command_reg; break;
        case CONTROL: return m_control_reg; break;
    }
    return 0;
}

void w65c51::write(u16 addr, u8 data) {
    switch (addr & 0x03) {
        case DATA:
            // Writing Data transmits it
            m_tx_buffer.push(data);
            // In emulation, Tx is instant. 
            // Real hardware would clear TxEmpty, wait, then set it.
            // We just leave TxEmpty (Bit 4) set to 1 (Ready).
            break;

        case STATUS: 
            // Soft Reset
            m_command_reg = 0x00;
            m_status_reg = ~0x80; 
            update_irq();
            break;
            
        case COMMAND: m_command_reg = data; update_irq(); break;
        case CONTROL: m_control_reg = data; break;
    }
}

void w65c51::rx_char(u8 c) {
    m_rx_buffer = c;
    m_status_reg |= 0x08; // Set Rx Full (Bit 3)
    m_status_reg |= 0x80; // Set IRQ Flag (Bit 7)
    update_irq();
}

void w65c51::update_irq() {
    // If IRQ Flag (Bit 7) is Set AND Command Reg Bit 1 is LOW (IRQ Enabled)
    bool irq_active = (m_status_reg & 0x80) && !(m_command_reg & 0x02);
    if (m_irq_cb) m_irq_cb(irq_active);
}

bool w65c51::has_tx_data() {
    return !m_tx_buffer.empty();
}

u8 w65c51::pop_tx_data() {
    if (m_tx_buffer.empty()) return 0;

    u8 data = m_tx_buffer.front();
    m_tx_buffer.pop();
    return data;
}