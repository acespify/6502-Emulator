#include "w65c22.h"
#include "../../emu/map.h"
#include <cstring>

// Bitmasks for IFR/IER
const u8 INT_CA2 = 0x01;
const u8 INT_CA1 = 0x02;
const u8 INT_SR  = 0x04;
const u8 INT_CB2 = 0x08;
const u8 INT_CB1 = 0x10;
const u8 INT_T2  = 0x20;
const u8 INT_T1  = 0x40;
const u8 INT_ANY = 0x80;

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
    m_latch_a = 0;
    m_latch_b = 0;

    // Default High (Pull-up)
    m_ca1_state = true;
    m_cb1_state = true;
    m_pb6_state = true;
    m_cb2_in_state = true;

    // Outputs default High
    m_ca2_out = true;
    m_cb2_out = true;
    m_ca2_pulse_active = false;
    m_cb2_pulse_active = false;

    // Timers
    m_t1_counter = 0xFFFF;
    m_t1_latch = 0xFFFF;
    m_t1_active = false;
    m_t1_pb7_state = true; // High by default

    m_t2_counter = 0xFFFF;
    m_t2_latch = 0xFFFF;
    m_t2_active = false;

    // Shift Register
    m_regs[SR] = 0;
    m_sr_count = 0;
    m_sr_running = false;
    m_cb2_lvl = 0;

    m_regs[IER] = 0x00; // Disable all interrupts
}

// ============================================================================
//  Clock needed for Timers functions
// ============================================================================
void w65c22::clock() {
    // Reset Pulse Mode pins after 1 cycle
    if (m_ca2_pulse_active) {
        m_ca2_out = true;
        m_ca2_pulse_active = false;
        update_control_outputs();
    }
    if (m_cb2_pulse_active) {
        m_cb2_out = true;
        m_cb2_pulse_active = false;
        update_control_outputs();
    }

    // --- TIMER 1 ---
    if (m_t1_active) {
        if (m_t1_counter == 0) {
            // Timer expired! Set IRQ flag (Bit 6)
            m_regs[IFR] |= INT_T1;

            // ACR Bit 6 determines mode: 0 = One-Shot, 1 = Free-Run
            if (m_regs[ACR] & 0x40) {
                // Free-Run: Reload from latch
                m_t1_counter = m_t1_latch;
                // ACR Bit 7: Toggle PB7 output
                if (m_regs[ACR] & 0x80) {
                    m_t1_pb7_state = !m_t1_pb7_state;
                    update_outputs();
                }
            } else {
                // One-Shot: Stop generating interrupts, roll over
                m_t1_active = false;
                m_t1_counter = 0xFFFF;
                // If PB7 was driven low by T1, it returns high on timeout
                if (m_regs[ACR] & 0x80) {
                    m_t1_pb7_state = true;
                    update_outputs();
                }
            }
            update_irq();
        } else {
            m_t1_counter--;
        }
    }

    // --- TIMER 2 ---
    // T2 decrements on PHI2 only if ACR Bit 5 is 0 (Interval Timer Mode)
    // If ACR Bit 5 is 1, it counts PB6 pulses (handled in set_pb6_input)
    if (m_t2_active && !(m_regs[ACR] & 0x20)) {
        if (m_t2_counter == 0) {
            m_regs[IFR] |= INT_T2; // Set IRQ flag (Bit 5)
            m_t2_active = false; // T2 is always one-shot (mostly)
            m_t2_counter = 0xFFFF;
            update_irq();
        } else {
            m_t2_counter--;
        }
    }

    // --- Shift Register (for now simplified) ---
    // In real hardware shifts based on T2 or Phi2.
    // Implement a simple placeholder logic for "Shift Out on Phi2"
    // ACR Bits 2-4 control SR mode.
    u8 sr_mode = (m_regs[ACR] >> 2) & 0x07;
    bool trigger_shift = false;// Modes 1, 2, 5, 6 use internal clocks (T2 or PHI2)

    // Mode 2 (Shift In) & 6 (Shift Out) use PHI2 rate
    if (m_sr_running) {
        if (sr_mode == 2 || sr_mode == 6){
            trigger_shift = true;
        }
        // Mode 1 (Shift In) & 5 (Shift Out) use Timer 2 rate
        else if ((sr_mode == 1 || sr_mode == 5) && m_t2_counter == 0){
            trigger_shift = true;
        }
    }
    if (trigger_shift) {
        // Shift Out (Modes 4, 5, 6, 7) - MSB goes to CB2
        if (sr_mode & 0x04) {
            m_cb2_out = (m_regs[SR] & 0x80) ? true : false;
            m_regs[SR] = (m_regs[SR] << 1) | (m_regs[SR] >> 7); // Rotate
            update_control_outputs();
        }
        // Shift In (Modes 0, 1, 2, 3) - CB2 goes to LSB
        else {
            u8 bit = m_cb2_in_state ? 1 : 0;
            m_regs[SR] = (m_regs[SR] << 1) | bit;
        }

        m_sr_count++;
        if (m_sr_count >= 8) {
            // Mode 2 & 6 stop after 8 bits. Mode 000 disables.
            if (sr_mode != 0) {
                m_regs[IFR] |= INT_SR;
                m_sr_running = false; // Stop
                update_irq();
            }
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

    // Port B Calculation
    // Get the standad GPIO output (ORB masked by DDRB)
    u8 standard_out = m_regs[ORB] & m_regs[DDRB];

    // Check for Timer 1 Override on Bit 7
    if (m_regs[ACR] & 0x80){
        // ACR Bit 7 is SET: Timer 1 controls PB7
        // We must mask out the GPIO bit 7 and OR in the Timer state.
        // Note: Timer 1 forces PB7 as output, overriding DDRB bit 7!
        standard_out &= 0x7F; // this clears Bit 7
        if (m_t1_pb7_state) {
            standard_out |= 0x80;   // Set Bit 7 if Timer says High
        }
    }

    if (m_port_b_cb) m_port_b_cb(standard_out);
    m_out_b = standard_out;
}

void w65c22::update_control_outputs() {
    // Notify external devices of CA2/CB2 state changes
    if (m_ca2_cb) m_ca2_cb(m_ca2_out);
    if (m_cb2_cb) m_cb2_cb(m_cb2_out);
}

void w65c22::update_irq() {
    // Check if any flag in IFR is set AND enabled in IER.
    // Bit 7 of IFR is usually "Any Active", but we check explicitly here.
    bool interrupt_active = (m_regs[IFR] & m_regs[IER] & 0x7F);
    
    if (interrupt_active){
        m_regs[IFR] |= INT_ANY;             // Set Top Bit
        if (m_irq_cb) m_irq_cb(false);      // Assert the IRQ setting it Low
    } else {
        m_regs[IFR] &= ~INT_ANY;            // Clear Top Bit
        if (m_irq_cb) m_irq_cb(true);       // This releases the IRQ to a High state
    }
}

// ============================================================================
//  Input Handling (Pins)
// ============================================================================
void w65c22::set_ca1(bool signal) {
    bool old_signal = m_ca1_state;
    m_ca1_state = signal;

    // Check for Active Edge based on PCR Bit 0
    bool active_edge = (m_regs[PCR] & 0x01) ? (!old_signal && signal) : (old_signal && !signal);
    
    if (active_edge) {
        // Set Interrupt Flag 
        m_regs[IFR] |= INT_CA1;

        // Latch Input if ACR bit 0 is set
        if (m_regs[ACR] & 0x01) {
            m_latch_a = m_in_a;
        }
        // Handshake Reset: If a Handshake Output mode, CA1 transition resets CA2
        if ((m_regs[PCR] & 0x0E) == 0x08){
            m_ca2_out = true;
            update_control_outputs();
        }
        update_irq();
    }
}

void w65c22::set_cb1(bool signal) {
    bool old_signal = m_cb1_state;
    m_cb1_state = signal;

    // PCR Bit 4 controls CB1 Active Edge
    // 0 = Negative Edge (High->Low), 1 = Positive Edge (Low->High)
    bool active_edge = (m_regs[PCR] & 0x10) ? (!old_signal && signal) : (old_signal && !signal);

    if (active_edge) {
        // 1. Set Interrupt Flag (Bit 4)
        m_regs[IFR] |= INT_CB1;
        
        // ACR Bit 1 enables latching for Port B
        if (m_regs[ACR] & 0x02) {
            m_latch_b = m_in_b;
        }

        // 2. Handle Write Handshake (Output Mode)
        // If we are in Handshake Output mode (PCR bits 7-5 = 100), 
        // an active edge on CB1 indicates the peripheral has taken the data.
        // We must reset CB2 to High.
        if ((m_regs[PCR] & 0xE0) == 0x80) { 
            m_cb2_out = true; 
            update_control_outputs();
        }
        update_irq();
    }
    // Note: External Shift Clock logic would attach here for Modes 3 & 7
}

void w65c22::set_cb2_input(bool signal) {
    // CB2 is only an input if PCR Bits 7-5 are 0xx (000, 001, 010, 011)
    if (m_regs[PCR] & 0x80) return; // Exit if in Output mode

    bool old_signal = m_cb2_in_state;
    m_cb2_in_state = signal;
    
    // PCR Bit 6 controls CB2 Active Edge (when in input mode)
    bool active_edge = (m_regs[PCR] & 0x40) ? (!old_signal && signal) : (old_signal && !signal);

    if (active_edge) {
        m_regs[IFR] |= INT_CB2; // Set Bit 3
        update_irq();
    }
}

void w65c22::set_pb6_input(bool signal) {
    bool old_signal = m_pb6_state; // You need to add m_pb6_state to header!
    m_pb6_state = signal;
    // Check if Timer 2 is in Pulse Counting Mode (ACR Bit 5 = 1) [cite: 346]
    if ((m_regs[ACR] & 0x20) && (old_signal && !signal)) {
        if (m_t2_counter == 0) {
            m_regs[IFR] |= INT_T2;
            m_t2_counter = 0xFFFF;
            // T2 continues to decrement (Active remains false to prevent clock decrement)
            update_irq();
        } else {
            m_t2_counter--;
        }
    }
}

// ============================================================================
//  Read Register
// ============================================================================
u8 w65c22::read(u16 addr) {
    u8 idx = addr & 0x0F;
    u8 val = 0;

    switch (idx) {
        case ORA: { // Reading Output Register A
            m_regs[IFR] &= ~INT_CA1;    // Clear flag

            u8 ca2_mode = (m_regs[PCR] >> 1) & 0x07;
            if (ca2_mode == 4) { // Handshake Output
                m_ca2_out = false;    
            } else if (ca2_mode == 5) { // Pulse Output
                m_ca2_out = false;
                m_ca2_pulse_active = true;  // Clock will reset this    
            }
            update_control_outputs();
            update_irq();
            [[fallthrough]];
        }
        case ORA_NH:{
            val = (m_regs[ACR] & 0x01) ? m_latch_a : (m_in_a & ~m_regs[DDRA]) | (m_regs[ORA] & m_regs[DDRA]);
            break;
        }
        case ORB: {// Read Port B
            m_regs[IFR] &= ~0x18; // Clear CB1/CB2 flags
            update_irq();
            val = (m_regs[ORB] & m_regs[DDRB]) | (m_in_b & ~m_regs[DDRB]);
            break;
        }
        // Timer 1   
        case T1CL: {// Read T1 Low
            m_regs[IFR] &= ~INT_T1;     // Clear T1 Interrupt
            update_irq();
            val = m_t1_counter & 0xFF;
            break;
        }
        case T1CH: { // Read T1 High
            val = (m_t1_counter >> 8) & 0xFF;
            break; 
        }
        case T1LL: val = m_t1_latch & 0xFF; break;
        case T1LH: val = (m_t1_latch >> 8) & 0xFF; break;
        // Timer 2
        case T2CL: { // Read T2 Low
            m_regs[IFR] &= ~INT_T2; // Clear T2 Interrupt
            update_irq();
            val = m_t2_counter & 0xFF;
            break;
        }
        case T2CH: { 
            val = (m_t2_counter >> 8) & 0xFF;
            break;
        }

        // Shift Register
        case SR: {
            val = m_regs[SR];
            m_regs[IFR] &= ~INT_SR;     // Clear SR Interrupt
            // Reading SR triggers shifting if in appropriate mode
            m_sr_running = true; 
            m_sr_count = 0;
            update_irq();
            break;
        }

        case IFR: val = m_regs[IFR]; break;
        case IER: val = m_regs[IER] | 0x80; break; // Bit 7 is always 1 reading IER?

        default: val = m_regs[idx]; break;
    }
    return val;
}

// ============================================================================
//  Write Register
// ============================================================================
void w65c22::write(u16 addr, u8 data) {
    u8 idx = addr & 0x0F;

    switch (idx) {
        case ORA:
        case ORA_NH: {
            m_regs[ORA] = data;
            update_outputs();
            if (idx == ORA) { 
                m_regs[IFR] &= ~(INT_CA1 | INT_CA2); 
                update_irq(); 
                // Write hanndshake Logic (CA2)
                u8 ca2_mode = (m_regs[PCR] >> 1) & 0x07;
                if (ca2_mode == 4 || ca2_mode == 5) {
                    m_ca2_out = false;
                    if (ca2_mode == 5) m_ca2_pulse_active = true;
                    update_control_outputs();
                }
            }
            break;
        }
        case ORB: {
            m_regs[ORB] = data;
            update_outputs();
            m_regs[IFR] &= ~(INT_CB1 | INT_CB2);
            
            // Write Handshake Logic (CB2)
            u8 cb2_mode = (m_regs[PCR] >> 5) & 0x07;
            if (cb2_mode == 4) { // Handshake
                m_cb2_out = false;    
            } else if (cb2_mode == 5) { // Pulse
                m_cb2_out = false;
                m_cb2_pulse_active = true;    
            }
            update_control_outputs();
            break;
        }
        case DDRA:
        case DDRB: {
            m_regs[idx] = data;
            update_outputs(); 
            break;
        }
        // Timer 1
        case T1CL:
        case T1LL: { 
            m_t1_latch = (m_t1_latch & 0xFF00) | data;
            break;
        }
        case T1CH: {// Writing high byte triggers load & start!
            m_t1_latch = (m_t1_latch & 0x00FF) | (data << 8);
            m_t1_counter = m_t1_latch;      // Load counter
            m_t1_active = true;             // Set active to start counting
            m_regs[IFR] &= ~INT_T1;         // Clear T1 interrupt

            // Validate for both One-Shot and Free-Run
            // If ACR7=1, writing T1C-H resets PB7 Interrupt flag and sets PB7 low.
            if (m_regs[ACR] & 0x80) {
                m_t1_pb7_state = false; // "PB7 will go to a Logic 0 following the load"
                update_outputs();
            }
            update_irq();
            break;
        }
        case T1LH: {
           m_t1_latch = (m_t1_latch & 0x00FF) | (data << 8);
           m_regs[IFR] &= ~INT_T1;
           update_irq();
           break; 
        } 

        // Timer 2
        case T2CL: {
            m_t2_latch = (m_t2_latch & 0xFF00) | data; 
            break;
        }
        case T2CH: {
            m_t2_latch = (m_t2_latch & 0x00FF) | (data << 8);
            m_t2_counter = m_t2_latch;
            m_t2_active = true;
            m_regs[IFR] &= ~INT_T2; 
            update_irq();
            break;
        }

        // Shift Register
        case SR: {
            m_regs[SR] = data;
            m_regs[IFR] &= ~INT_SR;     // Clear the Interrupt
            update_irq();
            // Start shifting 
            m_sr_running = true;
            m_sr_count = 0;
            break;
        }
        case IFR: {
            // Write 1 to clear bits
            m_regs[IFR] &= ~data;
            update_irq();
            break;
        }
        case IER: {
            // Bit 7 determines logic:
            // 1xxxxxxx = SET bits
            // 0xxxxxxx = CLEAR bits
            if (data & 0x80) { 
                m_regs[IER] |= (data & 0x7F); // Set
            } else {
                m_regs[IER] &= ~(data & 0x7F); // Clear
            }             
            update_irq();
            break;
        }
        default:
            m_regs[idx] = data;
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