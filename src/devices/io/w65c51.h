// ============================================================================
// Copyright (c) 2026 Andrew Young
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
// 
// ============================================================================


#pragma once
#include "../../emu/di_memory.h"
#include <functional>
#include <queue>

// ============================================================================
//  Device: W65C51N (ACIA)
// ============================================================================
class w65c51 : public device_memory_interface {
public:
    w65c51();

    void reset(); // Added RESET functionality to the component.

    // --- CPU Interface ---
    u8 read(u16 addr);
    void write(u16 addr, u8 data);
    void memory_map(address_map& map) override;

    // --- Serial Interface (The "MAX232" side) ---
    // Call this from Main/UI to send keyboard input to the 6502
    void rx_char(u8 c);
    
    // Read what the 6502 has transmitted (for the UI console)
    bool has_tx_data();
    u8 pop_tx_data();

    // --- Interrupts ---
    using irq_callback = std::function<void(bool state)>;
    void set_irq_callback(irq_callback cb) { m_irq_cb = cb; }

private:
    // Registers
    u8 m_data_reg;
    u8 m_status_reg;  // Bits: 7=IRQ, 4=TxEmpty 3=RxFull,
    u8 m_control_reg; // Baud rate (ignored in emulation)
    u8 m_command_reg;

    std::queue<u8> m_tx_buffer; // Outgoing (to PC) (Tx_Data)
    u8 m_rx_buffer;             // Incoming (from PC) (Rx_Data)

    irq_callback m_irq_cb;
    void update_irq();
};