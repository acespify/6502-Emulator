#pragma once

#include "emu/device.h"
#include "emu/di_execute.h"
#include "emu/di_memory.h"


// ============================================================================
//  W65C02S CPU Core (MAME-Architecture Compliant)
// ============================================================================
//  The Western Design Center 65C02S is a static core CMOS 8-bit microprocessor.
//  It extends the original MOS 6502 with new instructions, addressing modes,
//  and hardware pins (RDY, VPB, MLB, etc.).
// ============================================================================
class m6502_p : public device_t, 
                public device_execute_interface, 
                public device_memory_interface {
    public:

        // ========================================================================
        //  Public Constants (Hardware Pins)
        // ========================================================================
        // MAME treats lines as integers. These match standard pin definitions.
        static constexpr int IRQ_LINE   = 0;  // Maskable Interrupt (/IRQ)
        static constexpr int NMI_LINE   = 1;  // Non-Maskable Interrupt (/NMI)
        static constexpr int RESET_LINE = 2;  // Hardware Reset (/RES)
        static constexpr int RDY_LINE   = 3;  // Ready (Wait State Control)
        static constexpr int SO_LINE    = 4;  // Set Overflow (Rarely used input)
        // ========================================================================
        //  1. Construction
        // ========================================================================
        
        // WHAT: Constructor
        // WHEN: Created by the machine configuration.
        // WHY:  Sets up the device identity.
        // HOW:  Passes arguments up to the base device_t.
        m6502_p(machine_config &mconfig, const std::string &tag, device_t *owner, u32 clock);
      
        // ========================================================================
        //  2. Interface Overrides (The "Contracts")
        // ========================================================================

        // --- From device_t ---

        // WHAT: Hardware Power-On.
        // WHEN: Runs once at startup.
        // WHY:  We need to register our state for debugging (and save states later).
        virtual void device_start() override;

        // WHAT: Hardware Reset.
        // WHEN: Runs at startup and when the RESET button is pressed.
        // WHY:  The 6502 has a specific sequence: Read vector FFFC/FFFD -> Set PC.
        virtual void device_reset() override;

        // --- From device_execute_interface ---

        // WHAT: The Main Instruction Loop.
        // WHEN: Called by the Scheduler when it is this CPU's turn to run.
        // WHY:  This is the heartbeat. It fetches opcodes and executes them.
        virtual void execute_run() override;

        // --- From device_memory_interface ---
        
        // WHAT: Address Map Configuration.
        // WHEN: Called at startup.
        // WHY:  Allows the CPU to define its own internal map (rare for 6502, but required).
        // HOW:  Usually empty for 6502 as it relies on the motherboard map.
        virtual void memory_map(address_map &map) override;

        // Set the state of an input line (True=High, False=Low)
        void set_input_line(int line, bool state);

        // ========================================================================
        //  Getters used for UI
        // ========================================================================
        u16 get_pc()     const { return PC; }   // Program counter
        u8  get_sp()     const { return S; }    // Stack pointer
        u8  get_a()      const { return A; }    // Accumulator
        u8  get_x()      const { return X; }    // X register
        u8  get_y()      const { return Y; }    // Y register
        u8  get_flags()  const { return P; }    // Processor Status Register (Flags)

        // DEBUGGER HELPER
        // Reads a byte form the bus for visualization purposes.
        // We will use the internal 'space' interface if available, or just read_byte.
        u8 debug_peek(u16 addr) {
            // Since we are inside the class, we can access the protected read_byte.
            // In a real emulator,, we might use a specific "non-intrusive" read here.
            return read_byte(addr);
        }

        // Total cycles executed since reset (useful for timing/debugging)
        u64 total_cycles() const { return m_total_cycles; }

        // Setter for Cycle Budget (Used by Driver)
        void icount_set(int cycles) { m_icount = cycles; }
        int  icount_get() const { return m_icount; }

        // ========================================================================
        //  Internal Architecture
        // ========================================================================
    private:
        
        // ========================================================================
        //  3. Internal Registers (The CPU State)
        // ========================================================================
        //  WHAT: Variables representing the physical registers inside the chip.
        //  WHY:  These hold the data being processed.
        // ========================================================================

        u8  A = 0x00;   // Accumulator: Math and Logic operations go here.
        u8  X = 0x00;   // X Index: Loop counters and offsets.
        u8  Y = 0x00;   // Y Index: Loop counters and offsets.
        u8  S = 0x00;   // Stack Pointer: Points to the current location in the stack (page 1).
        u8  P = 0x00;   // Processor Status Register (Flags): Holds results (Zero, Negative, Carry).
        u16 PC = 0x0000;// Program Counter: The address of the NEXT instruction.

        // WHAT: Status Flag Enumeration.
        // WHY:  So we don't have to remember that "0x01" means Carry.
        enum Flags : u8 {
            C = (1 << 0), // Carry Bit
            Z = (1 << 1), // Zero Bit
            I = (1 << 2), // Interrupt Disable
            D = (1 << 3), // Decimal Mode
            B = (1 << 4), // Break command (Software Interrupt)
            U = (1 << 5), // Unused (always 1)
            V = (1 << 6), // Overflow 1 = true
            N = (1 << 7)  // Negative 1 = neg
        };

        // Emulation state
        int m_icount = 0;           // Cycles remaining in this timeslice
        u64 m_total_cycles = 0;     // Total cycles since power-on

        // Interrupt Lines state
        bool m_irq_line = false;        // Default High
        bool m_nmi_line = false;        // Active Low
        bool m_nmi_prev  = false;       // To detect falling edge
        bool m_rdy_line  = true;        // Ready is usally High (Active)
        bool m_reset_line = false;
        
        // ========================================================================
        //  Internal State for Addressing Modes
        // ========================================================================
        u16 addr_abs    = 0x0000;   // Effective Address
        u16 addr_rel    = 0x0000;   // Branch Offset
        u8  fetched     = 0x00;     // Operand Latch
        u8  opcode      = 0x00;     // Instruction Latch
        u8  m_cycles      = 0;        // Current instruction cycle cost

        // ========================================================================
        //  Addressing Modes
        // ========================================================================
        u8 IMP(); u8 IMM(); 
        u8 ZP0(); u8 ZPX(); u8 ZPY(); u8 ZPI(); // Zero Page variants
        u8 ABS(); u8 ABX(); u8 ABY();           // Absolute variants
        u8 IND(); u8 IZX(); u8 IZY(); u8 IAX(); // Indirect variants
        u8 REL();                               // Relative

        // ========================================================================
        //  Opcodes (W65C02S Complete Set)
        // ========================================================================
        // Load/Store/Move
        void LDA(); void LDX(); void LDY();
        void STA(); void STX(); void STY(); void STZ(); // STZ is CMOS specific
        void TAX(); void TAY(); void TXA(); void TYA(); void TSX(); void TXS();

        // Arithmetic / Logic
        void ADC(); void SBC();
        void AND(); void EOR(); void ORA(); void BIT();
        void ASL(); void LSR(); void ROL(); void ROR();
        void INC(); void DEC(); void INX(); void DEX(); void INY(); void DEY();
        void CMP(); void CPX(); void CPY();
        void TRB(); void TSB(); // CMOS Test/Reset/Set Bits

        // Control Flow
        void JMP(); void JSR(); void RTS(); 
        void BCC(); void BCS(); void BEQ(); void BMI(); 
        void BNE(); void BPL(); void BVC(); void BVS();
        void BRA(); // CMOS Branch Always

        // Stack
        void PHA(); void PLA(); void PHP(); void PLP();
        void PHX(); void PLX(); void PHY(); void PLY(); // CMOS Push/Pull Index

        // System
        void BRK(); void RTI(); void NOP(); 
        void CLC(); void SEC(); void CLI(); void SEI(); 
        void CLV(); void CLD(); void SED();
        
        // Illegal / Unimplemented (NOPs on CMOS usually)
        void XXX(); 

        // ========================================================================
        //  Helpers
        // ========================================================================
        u8   fetch();
        u8 fetch_data();
        void branch_exec(bool cond);

        void set_flag(Flags f, bool v);
        u8   get_flag(Flags f);
        
        void push_byte(u8 v);
        u8   pop_byte();
        void push_word(u16 v);
        u16  pop_word();

        // Interrupt Logic
        void irq(); 
        void nmi();
        
        // ========================================================================
        //  Instruction Table
        // ========================================================================
        struct Instruction {
            void (m6502_p::*operate)(void) = nullptr;
            u8   (m6502_p::*addrmode)(void) = nullptr;
            u8   cycles = 0;
        };

        // The lookup Table (Fixed size array)
        Instruction lookup[256];
};