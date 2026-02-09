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
        ORB = 0, ORA = 1,
        DDRB = 2, DDRA = 3,
        T1CL = 4, T1CH = 5, T1LL = 6, T1LH = 7,
        T2CL = 8, T2CH = 9,
        SR = 0x0A, ACR = 0x0B, PCR = 0x0C,
        IFR = 0x0D, IER = 0x0E,
        ORA_NH = 0x0F // ORA No-Handshake
    };

    // ----- CALLBACKS -----
    // Definition of a line callback ( State: 1 = High/Clear, o = Low/Assert)
    using irq_callback = std::function<void(bool)>;
    // Defines a function type that accepts a byte (u8)
    using port_callback = std::function<void(u8)>;

    // --- HARDWARE INTERFACE ---
    // The CPU calls these to read/write the VIA's internal registers.
    u8 read(u16 addr);
    void write(u16 addr, u8 data);
    
    // System Reset (RESB pin)
    void reset();

    // Debugger Helper: Read without side effects
    u8 peek(u16 addr) const {
        // just return the raw register value
        // No "Clear Interrupt" logic here.
        return m_regs[addr & 0x0F];
    }

    // --- PORT INTERFACE ---
    // These functions simulate the physical pins on the side of the chip.
    // External devices (like the LCD) will call these or read these.
    
    // Set Input Signals (Simulating buttons/sensors connected to VIA)
    void set_port_a_input(u8 data) { m_in_a = data; }
    void set_port_b_input(u8 data) { m_in_b = data; }

    // Read Output Signals (Simulating LEDs/LCD connected to VIA)
    u8 get_port_a_output() { return m_out_a; }
    u8 get_port_b_output() { return m_out_b; }

    // Setter for the callback
    void set_irq_callback(irq_callback cb) { m_irq_cb = cb;}
    
    // Setters for the callbacks (Call these in the Driver)
    void set_port_a_callback(port_callback cb) { m_port_a_cb = cb; }
    void set_port_b_callback(port_callback cb) { m_port_b_cb = cb; }

    // Required by device interface
    void memory_map(address_map& map) override;

    
    

    

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
    //  Input Latches (Data coming FROM the outside world)
    u8 m_in_a;
    u8 m_in_b;
    
    // Output Latches (Data sent TO the outside world)
    u8 m_out_a;
    u8 m_out_b;

    // The stored callback function
    irq_callback m_irq_cb;
    port_callback m_port_a_cb;
    port_callback m_port_b_cb;
    
    // Helper to update the physical output pins based on DDR and Regs
    void update_outputs();
    // Internal helper to trigger it
    void update_irq();
};