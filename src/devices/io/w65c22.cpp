#include "w65c22.h"
#include "../../emu/map.h"
#include <cstring>


// ============================================================================
//  Constructor & Reset
// ============================================================================
w65c22::w65c22() {
    reset();
}

void w65c22::reset() {
    std::memset(m_regs, 0x00, 16);
    m_in_a = 0xFF; 
    m_in_b = 0xFF;
    m_out_a = 0;
    m_out_b = 0;

    m_t1_counter = 0xFFFF;
    m_t1_latch = 0xFFFF;
    m_t1_active = false;
    
    m_t2_counter = 0xFFFF;
    m_t2_latch = 0xFFFF;
    m_t2_active = false;
}

// ============================================================================
//  Clock needed for Timers functions
// ============================================================================
void w65c22::clock() {
    // --- TIMER 1 ---
    if (m_t1_active) {
        if (m_t1_counter == 0) {
            // Timer expired! Set IRQ flag (Bit 6)
            m_regs[IFR] |= 0x40;
            update_irq();

            // Reload logic depends on ACR Bit 6 (Free-run vs One-shot)
            if (m_regs[ACR] & 0x40) {
                m_t1_counter = m_t1_latch; // Free-run
            } else {
                m_t1_active = false; // One-shot stop
                m_t1_counter = 0xFFFF;
            }
        } else {
            m_t1_counter--;
        }
    }

    // --- TIMER 2 ---
    if (m_t2_active) {
        if (m_t2_counter == 0) {
            m_regs[IFR] |= 0x20; // Set IRQ flag (Bit 5)
            update_irq();
            m_t2_active = false; // T2 is always one-shot (mostly)
            m_t2_counter = 0xFFFF;
        } else {
            m_t2_counter--;
        }
    }
}

// ============================================================================
//  Internal Logic: Update Outputs & IRQs
// ============================================================================
void w65c22::update_outputs() {
    // 1. Calculate Port A
    u8 new_out_a = m_regs[ORA] & m_regs[DDRA];
    if (m_port_a_cb) {
        m_port_a_cb(new_out_a);
    }
    m_out_a = new_out_a;

    // 2. Calculate Port B
    u8 new_out_b = m_regs[ORB] & m_regs[DDRB];
    if (m_port_b_cb) {
        m_port_b_cb(new_out_b);
    }
    m_out_b = new_out_b;
}

// THIS IS THE MISSING FUNCTION BODY
void w65c22::update_irq() {
    // Check if any flag in IFR is set AND enabled in IER.
    // Bit 7 of IFR is usually "Any Active", but we check explicitly here.
    bool interrupt_active = (m_regs[IFR] & m_regs[IER] & 0x7F);
    
    if (m_irq_cb) {
        // Active LOW logic: 
        // If active (true), send FALSE (0). If clear (false), send TRUE (1).
        m_irq_cb(!interrupt_active);
    }
}

// ============================================================================
//  Read Register
// ============================================================================
u8 w65c22::read(u16 addr) {
    u8 reg_index = addr & 0x0F;
    u8 val = 0;

    switch (reg_index) {
        case ORA: // Reading Output Register A
            // Clears Interrupts!
            m_regs[IFR] &= ~0x03; // Clear CA1/CA2 flags (simplification)
            update_irq();
            // Fallthrough to normal logic
        case ORA_NH:
            val = (m_regs[ORA] & m_regs[DDRA]) | (m_in_a & ~m_regs[DDRA]);
            break;

        case ORB: 
            m_regs[IFR] &= ~0x18; // Clear CB1/CB2 flags
            update_irq();
            val = (m_regs[ORB] & m_regs[DDRB]) | (m_in_b & ~m_regs[DDRB]);
            break;
            
        case T1CL: val = m_t1_counter & 0xFF; m_regs[IFR] &= ~0x40; update_irq(); break;
        case T1CH: val = m_t1_counter >> 8; break;
        case T2CL: val = m_t2_counter & 0xFF; m_regs[IFR] &= ~0x20; update_irq(); break;
        case T2CH: val = m_t2_counter >> 8; break;

        case IFR: val = m_regs[IFR]; break;
        case IER: val = m_regs[IER] | 0x80; break; // Bit 7 is always 1 reading IER?

        default: val = m_regs[reg_index]; break;
    }
    return val;
}

// ============================================================================
//  Write Register
// ============================================================================
void w65c22::write(u16 addr, u8 data) {
    u8 reg_index = addr & 0x0F;

    switch (reg_index) {
        case ORA:
        case ORA_NH:
            m_regs[ORA] = data;
            update_outputs();
            if (reg_index == ORA) { m_regs[IFR] &= ~0x03; update_irq(); }
            break;

        case ORB:
            m_regs[ORB] = data;
            update_outputs();
            m_regs[IFR] &= ~0x18; update_irq();
            break;

        case DDRA:
        case DDRB:
            m_regs[reg_index] = data;
            update_outputs(); 
            break;
            
        // Timer 1
        case T1CL: 
            m_t1_latch = (m_t1_latch & 0xFF00) | data; 
            break;
        case T1CH: // Writing high byte triggers load & start!
            m_t1_latch = (m_t1_latch & 0x00FF) | (data << 8);
            m_t1_counter = m_t1_latch;
            m_t1_active = true;
            m_regs[IFR] &= ~0x40; // Clear T1 interrupt
            update_irq();
            break;
        case T1LL: m_t1_latch = (m_t1_latch & 0xFF00) | data; break;
        case T1LH: m_t1_latch = (m_t1_latch & 0x00FF) | (data << 8); m_regs[IFR] &= ~0x40; update_irq(); break;

        // Timer 2
        case T2CL: 
            m_t2_latch = (m_t2_latch & 0xFF00) | data; 
            break;
        case T2CH: 
            m_t2_latch = (m_t2_latch & 0x00FF) | (data << 8);
            m_t2_counter = m_t2_latch;
            m_t2_active = true;
            m_regs[IFR] &= ~0x20; 
            update_irq();
            break;

        case IFR:
            // Write 1 to clear bits
            m_regs[IFR] &= ~data;
            update_irq();
            break;

        case IER:
            if (data & 0x80) m_regs[IER] |= (data & 0x7F); // Set
            else             m_regs[IER] &= ~(data & 0x7F); // Clear
            update_irq();
            break;

        default:
            m_regs[reg_index] = data;
            break;
    }
}

// ============================================================================
//  Map Installation
// ============================================================================
void w65c22::memory_map(address_map& map) {
    map.install(0x0000, 0xFFFF, 
        [this](u16 addr) { return this->read(addr); },
        [this](u16 addr, u8 data) { this->write(addr, data); }
    );
}