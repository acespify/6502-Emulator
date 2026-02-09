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
        case ORA: 
        case ORA_NH:
            val = (m_regs[ORA] & m_regs[DDRA]) | (m_in_a & ~m_regs[DDRA]);
            if (reg_index == ORA) {
                // TODO: Clear CA1/CA2 interrupt flags in IFR
                update_irq();
            }
            break;

        case ORB: 
            val = (m_regs[ORB] & m_regs[DDRB]) | (m_in_b & ~m_regs[DDRB]);
            if (reg_index == ORB) {
                // TODO: Clear CB1/CB2 interrupts
                update_irq();
            }
            break;
            
        case IFR: 
            // Update the "Any Interrupt" bit (Bit 7) dynamically
            val = m_regs[IFR];
            if (val & 0x7F) val |= 0x80; else val &= 0x7F;
            break;

        default:
            val = m_regs[reg_index];
            break;
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
            break;

        case ORB:
            m_regs[ORB] = data;
            update_outputs();
            break;

        case DDRA:
        case DDRB:
            m_regs[reg_index] = data;
            update_outputs(); 
            break;
            
        case IFR:
            // Write 1 to clear bits
            m_regs[IFR] &= ~data;
            update_irq();
            break;

        case IER:
            // Bit 7 determines Set (1) or Clear (0)
            if (data & 0x80) {
                m_regs[IER] |= (data & 0x7F);
            } else {
                m_regs[IER] &= ~(data & 0x7F);
            }
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