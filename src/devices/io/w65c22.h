#pragma once
#include "../../emu/di_memory.h"
#include <string>
#include <functional>

// ============================================================================
//  Device: W65C22 (Versatile Interface Adapter)
// ============================================================================
//  WHO:  The I/O Controller.
//  WHAT: Provides 2 Parallel Ports (PA/PB) and 2 Timers.
//  WHY:  The 6502 has no I/O pins. It needs this chip to talk to the LCD/Buttons.
// ============================================================================
class w65c22 : public device_memory_interface {
public:
    w65c22();
    virtual ~w65c22() = default;

    // Register Offsets (for readability)
    enum Regs {
        ORB = 0, ORA = 1, DDRB = 2, DDRA = 3,
        T1CL = 4, T1CH = 5, T1LL = 6, T1LH = 7,
        T2CL = 8, T2CH = 9, SR = 0x0A, ACR = 0x0B,
        PCR = 0x0C, IFR = 0x0D, IER = 0x0E, ORA_NH = 0x0F// ORA No-Handshake
    };

    // ----- CALLBACKS -----
    using irq_callback = std::function<void(bool)>;     // State: 1 = High/Clear, 0 = Low/Assert
    using port_callback = std::function<void(u8)>;      // Sends a byte to external device
    using line_callback = std::function<void(bool)>;    // Control line update (CA2/CB2)

    // --- HARDWARE INTERFACE ---
    // The CPU calls these to read/write the VIA's internal registers.
    u8 read(u16 addr);
    void write(u16 addr, u8 data); 
    void reset();   // System Reset (RESB pin)
    void clock();   // Must be called every CPU Cycle

    // --- PORT INTERFACE ---
    // Inputs from external world
    void set_port_a_input(u8 data) { m_in_a = data; }
    void set_port_b_input(u8 data) { m_in_b = data; }

    // Function to handle external signal changes on the control lines
    void set_ca1(bool signal);              // CA1 is always input
    void set_cb1(bool signal);              // CB1 is input (or SR Clock)
    void set_cb2_input(bool signal);        // CB2 is input only in specific PCR modes
    void set_pb6_input(bool signal);        // PB6 input for Timer 2 Pulse Counting

    // Get Output States
    u8 get_port_a_output() { return m_out_a; }
    u8 get_port_b_output() { return m_out_b; }

    // Setter for the callback
    void set_irq_callback(irq_callback cb) { m_irq_cb = cb;}
    void set_ca2_callback(line_callback cb) { m_ca2_cb = cb; }
    void set_cb2_callback(line_callback cb) { m_cb2_cb = cb; }
    void set_port_a_callback(port_callback cb) { m_port_a_cb = cb; }
    void set_port_b_callback(port_callback cb) { m_port_b_cb = cb; }

    // Required by device interface
    void memory_map(address_map& map) override;

    // Debugger Helper: Read without side effects
    u8 peek(u16 addr) const { return m_regs[addr & 0x0F]; }

private:
    // ========================================================================
    //  Internal Registers (The "Memory" of the VIA)
    // ========================================================================
    //  0: ORB (Output B) / IRB (Input B)
    //  1: ORA (Output A) / IRA (Input A)
    //  2: DDRB (Direction B)
    //  3: DDRA (Direction A)
    //  4-9: Timers (T1C, T1L, T2C)
    //  A: SR (Shift Reg)
    //  B: ACR (Aux Control)
    //  C: PCR (Peripheral Control)
    //  D: IFR (Interrupt Flag)
    //  E: IER (Interrupt Enable)
    //  F: ORA (No Handshake)
    u8 m_regs[16];

    // ========================================================================
    //  Port Latches
    // ========================================================================
    u8 m_in_a, m_in_b, m_out_a, m_out_b;
    u8 m_latch_a, m_latch_b;

    // Control Line States
    bool m_ca1_state, m_cb1_state;                  // Current input state
    bool m_ca2_out, m_cb2_out;                      // Current output state
    bool m_cb2_in_state;                            // Current input state for CB2
    bool m_ca2_pulse_active, m_cb2_pulse_active;    // Pulse Mode Tracking
    bool m_pb6_state;                               // PB6 state for Pulse Counting

    // Timer States
    u16 m_t1_counter, m_t1_latch;
    u16 m_t2_counter, m_t2_latch;
    bool m_t1_active, m_t2_active;
    bool m_t1_pb7_state;  // Logic state of the PB7 override

    // Shift Register State
    u8 m_sr_count;          // Bits shifted so far
    bool m_sr_running;      // Is shifting active?
    u8 m_cb2_lvl;           // Simulated CB2 output level

    // Callbacks
    irq_callback m_irq_cb;
    line_callback m_ca2_cb, m_cb2_cb;
    port_callback m_port_a_cb, m_port_b_cb;

    // Helpers
    void update_outputs();          // Update PA/PB pins
    void update_control_outputs();  // Update CA2/CB2 pins
    void update_irq();              // Update IRQ line based on IFR & IER
};