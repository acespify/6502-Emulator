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
    u8 m_status_reg;  // Bits: 7=IRQ, 4=RxFull, 1=TxEmpty
    u8 m_command_reg; // Controls IRQ enables
    u8 m_control_reg; // Baud rate (ignored in emulation)

    std::queue<u8> m_tx_buffer; // Outgoing (to PC)
    u8 m_rx_buffer;             // Incoming (from PC)

    irq_callback m_irq_cb;
    void update_irq();
};