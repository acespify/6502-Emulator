#include "m6502.h"
#include "ui/views/debug_view.h"
#include <iostream>

// ============================================================================
//  Constructor
// ============================================================================
m6502_p::m6502_p(machine_config &mconfig, const std::string &tag, device_t *owner, u32 clock)
    : device_t(mconfig, tag, owner, clock),
      device_execute_interface(),
      device_memory_interface()
{
    // ------------------------------------------------------------------------
    //  Initialize the Lookup Table
    // ------------------------------------------------------------------------
    // Default all to XXX (Illegal)
    for (int i = 0; i < 256; i++) {
        lookup[i] = { &m6502_p::XXX, &m6502_p::IMP, 1 };
    }

    // ========================================================================
    //  Standard 6502 Opcodes
    // ========================================================================
    
    // LDA
    lookup[0xA9] = { &m6502_p::LDA, &m6502_p::IMM, 2 };
    lookup[0xA5] = { &m6502_p::LDA, &m6502_p::ZP0, 3 };
    lookup[0xB5] = { &m6502_p::LDA, &m6502_p::ZPX, 4 };
    lookup[0xAD] = { &m6502_p::LDA, &m6502_p::ABS, 4 };
    lookup[0xBD] = { &m6502_p::LDA, &m6502_p::ABX, 4 }; // +1 if page cross
    lookup[0xB9] = { &m6502_p::LDA, &m6502_p::ABY, 4 }; // +1 if page cross
    lookup[0xA1] = { &m6502_p::LDA, &m6502_p::IZX, 6 };
    lookup[0xB1] = { &m6502_p::LDA, &m6502_p::IZY, 5 }; // +1 if page cross
    lookup[0xB2] = { &m6502_p::LDA, &m6502_p::ZPI, 5 }; // W65C02S Exclusive

    // LDX
    lookup[0xA2] = { &m6502_p::LDX, &m6502_p::IMM, 2 };
    lookup[0xA6] = { &m6502_p::LDX, &m6502_p::ZP0, 3 };
    lookup[0xB6] = { &m6502_p::LDX, &m6502_p::ZPY, 4 };
    lookup[0xAE] = { &m6502_p::LDX, &m6502_p::ABS, 4 };
    lookup[0xBE] = { &m6502_p::LDX, &m6502_p::ABY, 4 }; // +1 if page cross

    // LDY
    lookup[0xA0] = { &m6502_p::LDY, &m6502_p::IMM, 2 };
    lookup[0xA4] = { &m6502_p::LDY, &m6502_p::ZP0, 3 };
    lookup[0xB4] = { &m6502_p::LDY, &m6502_p::ZPX, 4 };
    lookup[0xAC] = { &m6502_p::LDY, &m6502_p::ABS, 4 };
    lookup[0xBC] = { &m6502_p::LDY, &m6502_p::ABX, 4 }; // +1 if page cross

    // STA
    lookup[0x85] = { &m6502_p::STA, &m6502_p::ZP0, 3 };
    lookup[0x95] = { &m6502_p::STA, &m6502_p::ZPX, 4 };
    lookup[0x8D] = { &m6502_p::STA, &m6502_p::ABS, 4 };
    lookup[0x9D] = { &m6502_p::STA, &m6502_p::ABX, 5 };
    lookup[0x99] = { &m6502_p::STA, &m6502_p::ABY, 5 };
    lookup[0x81] = { &m6502_p::STA, &m6502_p::IZX, 6 };
    lookup[0x91] = { &m6502_p::STA, &m6502_p::IZY, 6 };
    lookup[0x92] = { &m6502_p::STA, &m6502_p::ZPI, 5 }; // W65C02S Exclusive

    // STX
    lookup[0x86] = { &m6502_p::STX, &m6502_p::ZP0, 3 };
    lookup[0x96] = { &m6502_p::STX, &m6502_p::ZPY, 4 };
    lookup[0x8E] = { &m6502_p::STX, &m6502_p::ABS, 4 };

    // STY
    lookup[0x84] = { &m6502_p::STY, &m6502_p::ZP0, 3 };
    lookup[0x94] = { &m6502_p::STY, &m6502_p::ZPX, 4 };
    lookup[0x8C] = { &m6502_p::STY, &m6502_p::ABS, 4 };

    // STZ (Store Zero) - W65C02S Exclusive
    lookup[0x64] = { &m6502_p::STZ, &m6502_p::ZP0, 3 };
    lookup[0x74] = { &m6502_p::STZ, &m6502_p::ZPX, 4 };
    lookup[0x9C] = { &m6502_p::STZ, &m6502_p::ABS, 4 };
    lookup[0x9E] = { &m6502_p::STZ, &m6502_p::ABX, 5 };

    // TRANSFERS
    lookup[0xAA] = { &m6502_p::TAX, &m6502_p::IMP, 2 };
    lookup[0xA8] = { &m6502_p::TAY, &m6502_p::IMP, 2 };
    lookup[0x8A] = { &m6502_p::TXA, &m6502_p::IMP, 2 };
    lookup[0x98] = { &m6502_p::TYA, &m6502_p::IMP, 2 };
    lookup[0x9A] = { &m6502_p::TXS, &m6502_p::IMP, 2 };
    lookup[0xBA] = { &m6502_p::TSX, &m6502_p::IMP, 2 };

    // STACK
    lookup[0x48] = { &m6502_p::PHA, &m6502_p::IMP, 3 };
    lookup[0x68] = { &m6502_p::PLA, &m6502_p::IMP, 4 };
    lookup[0x08] = { &m6502_p::PHP, &m6502_p::IMP, 3 };
    lookup[0x28] = { &m6502_p::PLP, &m6502_p::IMP, 4 };
    
    // W65C02S Stack Extensions
    lookup[0xDA] = { &m6502_p::PHX, &m6502_p::IMP, 3 };
    lookup[0x5A] = { &m6502_p::PHY, &m6502_p::IMP, 3 };
    lookup[0xFA] = { &m6502_p::PLX, &m6502_p::IMP, 4 };
    lookup[0x7A] = { &m6502_p::PLY, &m6502_p::IMP, 4 };

    // MATH (ADC/SBC)
    lookup[0x69] = { &m6502_p::ADC, &m6502_p::IMM, 2 };
    lookup[0x65] = { &m6502_p::ADC, &m6502_p::ZP0, 3 };
    lookup[0x75] = { &m6502_p::ADC, &m6502_p::ZPX, 4 };
    lookup[0x6D] = { &m6502_p::ADC, &m6502_p::ABS, 4 };
    lookup[0x7D] = { &m6502_p::ADC, &m6502_p::ABX, 4 }; // +1 page cross
    lookup[0x79] = { &m6502_p::ADC, &m6502_p::ABY, 4 }; // +1 page cross
    lookup[0x61] = { &m6502_p::ADC, &m6502_p::IZX, 6 };
    lookup[0x71] = { &m6502_p::ADC, &m6502_p::IZY, 5 }; // +1 page cross
    lookup[0x72] = { &m6502_p::ADC, &m6502_p::ZPI, 5 };

    lookup[0xE9] = { &m6502_p::SBC, &m6502_p::IMM, 2 };
    lookup[0xE5] = { &m6502_p::SBC, &m6502_p::ZP0, 3 };
    lookup[0xF5] = { &m6502_p::SBC, &m6502_p::ZPX, 4 };
    lookup[0xED] = { &m6502_p::SBC, &m6502_p::ABS, 4 };
    lookup[0xFD] = { &m6502_p::SBC, &m6502_p::ABX, 4 }; // +1 page cross
    lookup[0xF9] = { &m6502_p::SBC, &m6502_p::ABY, 4 }; // +1 page cross
    lookup[0xE1] = { &m6502_p::SBC, &m6502_p::IZX, 6 };
    lookup[0xF1] = { &m6502_p::SBC, &m6502_p::IZY, 5 }; // +1 page cross
    lookup[0xF2] = { &m6502_p::SBC, &m6502_p::ZPI, 5 };

    // LOGIC (AND, EOR, ORA, BIT)
    lookup[0x29] = { &m6502_p::AND, &m6502_p::IMM, 2 };
    lookup[0x25] = { &m6502_p::AND, &m6502_p::ZP0, 3 };
    lookup[0x35] = { &m6502_p::AND, &m6502_p::ZPX, 4 };
    lookup[0x2D] = { &m6502_p::AND, &m6502_p::ABS, 4 };
    lookup[0x3D] = { &m6502_p::AND, &m6502_p::ABX, 4 }; // +1 page cross
    lookup[0x39] = { &m6502_p::AND, &m6502_p::ABY, 4 }; // +1 page cross
    lookup[0x21] = { &m6502_p::AND, &m6502_p::IZX, 6 };
    lookup[0x31] = { &m6502_p::AND, &m6502_p::IZY, 5 }; // +1 page cross
    lookup[0x32] = { &m6502_p::AND, &m6502_p::ZPI, 5 };

    lookup[0x49] = { &m6502_p::EOR, &m6502_p::IMM, 2 };
    lookup[0x45] = { &m6502_p::EOR, &m6502_p::ZP0, 3 };
    lookup[0x55] = { &m6502_p::EOR, &m6502_p::ZPX, 4 };
    lookup[0x4D] = { &m6502_p::EOR, &m6502_p::ABS, 4 };
    lookup[0x5D] = { &m6502_p::EOR, &m6502_p::ABX, 4 }; // +1 page cross
    lookup[0x59] = { &m6502_p::EOR, &m6502_p::ABY, 4 }; // +1 page cross
    lookup[0x41] = { &m6502_p::EOR, &m6502_p::IZX, 6 };
    lookup[0x51] = { &m6502_p::EOR, &m6502_p::IZY, 5 }; // +1 page cross
    lookup[0x52] = { &m6502_p::EOR, &m6502_p::ZPI, 5 };

    lookup[0x09] = { &m6502_p::ORA, &m6502_p::IMM, 2 };
    lookup[0x05] = { &m6502_p::ORA, &m6502_p::ZP0, 3 };
    lookup[0x15] = { &m6502_p::ORA, &m6502_p::ZPX, 4 };
    lookup[0x0D] = { &m6502_p::ORA, &m6502_p::ABS, 4 };
    lookup[0x1D] = { &m6502_p::ORA, &m6502_p::ABX, 4 }; // +1 page cross
    lookup[0x19] = { &m6502_p::ORA, &m6502_p::ABY, 4 }; // +1 page cross
    lookup[0x01] = { &m6502_p::ORA, &m6502_p::IZX, 6 };
    lookup[0x11] = { &m6502_p::ORA, &m6502_p::IZY, 5 }; // +1 page cross
    lookup[0x12] = { &m6502_p::ORA, &m6502_p::ZPI, 5 };

    lookup[0x24] = { &m6502_p::BIT, &m6502_p::ZP0, 3 };
    lookup[0x2C] = { &m6502_p::BIT, &m6502_p::ABS, 4 };
    lookup[0x89] = { &m6502_p::BIT, &m6502_p::IMM, 2 }; // W65C02S
    lookup[0x34] = { &m6502_p::BIT, &m6502_p::ZPX, 4 }; // W65C02S
    lookup[0x3C] = { &m6502_p::BIT, &m6502_p::ABX, 4 }; // W65C02S

    // W65C02S Bit Manipulation (TRB/TSB)
    lookup[0x14] = { &m6502_p::TRB, &m6502_p::ZP0, 5 };
    lookup[0x1C] = { &m6502_p::TRB, &m6502_p::ABS, 6 };
    lookup[0x04] = { &m6502_p::TSB, &m6502_p::ZP0, 5 };
    lookup[0x0C] = { &m6502_p::TSB, &m6502_p::ABS, 6 };

    // SHIFTS (ASL, LSR, ROL, ROR)
    lookup[0x0A] = { &m6502_p::ASL, &m6502_p::IMP, 2 };
    lookup[0x06] = { &m6502_p::ASL, &m6502_p::ZP0, 5 };
    lookup[0x16] = { &m6502_p::ASL, &m6502_p::ZPX, 6 };
    lookup[0x0E] = { &m6502_p::ASL, &m6502_p::ABS, 6 };
    lookup[0x1E] = { &m6502_p::ASL, &m6502_p::ABX, 7 };

    lookup[0x4A] = { &m6502_p::LSR, &m6502_p::IMP, 2 };
    lookup[0x46] = { &m6502_p::LSR, &m6502_p::ZP0, 5 };
    lookup[0x56] = { &m6502_p::LSR, &m6502_p::ZPX, 6 };
    lookup[0x4E] = { &m6502_p::LSR, &m6502_p::ABS, 6 };
    lookup[0x5E] = { &m6502_p::LSR, &m6502_p::ABX, 7 };

    lookup[0x2A] = { &m6502_p::ROL, &m6502_p::IMP, 2 };
    lookup[0x26] = { &m6502_p::ROL, &m6502_p::ZP0, 5 };
    lookup[0x36] = { &m6502_p::ROL, &m6502_p::ZPX, 6 };
    lookup[0x2E] = { &m6502_p::ROL, &m6502_p::ABS, 6 };
    lookup[0x3E] = { &m6502_p::ROL, &m6502_p::ABX, 7 };

    lookup[0x6A] = { &m6502_p::ROR, &m6502_p::IMP, 2 };
    lookup[0x66] = { &m6502_p::ROR, &m6502_p::ZP0, 5 };
    lookup[0x76] = { &m6502_p::ROR, &m6502_p::ZPX, 6 };
    lookup[0x6E] = { &m6502_p::ROR, &m6502_p::ABS, 6 };
    lookup[0x7E] = { &m6502_p::ROR, &m6502_p::ABX, 7 };

    // INC/DEC
    lookup[0xE6] = { &m6502_p::INC, &m6502_p::ZP0, 5 };
    lookup[0xF6] = { &m6502_p::INC, &m6502_p::ZPX, 6 };
    lookup[0xEE] = { &m6502_p::INC, &m6502_p::ABS, 6 };
    lookup[0xFE] = { &m6502_p::INC, &m6502_p::ABX, 7 };
    lookup[0xE8] = { &m6502_p::INX, &m6502_p::IMP, 2 };
    lookup[0xC8] = { &m6502_p::INY, &m6502_p::IMP, 2 };
    lookup[0x1A] = { &m6502_p::INC, &m6502_p::IMP, 2 }; // INC A (W65C02S)
    lookup[0x3A] = { &m6502_p::DEC, &m6502_p::IMP, 2 }; // DEC A (W65C02S)

    lookup[0xC6] = { &m6502_p::DEC, &m6502_p::ZP0, 5 };
    lookup[0xD6] = { &m6502_p::DEC, &m6502_p::ZPX, 6 };
    lookup[0xCE] = { &m6502_p::DEC, &m6502_p::ABS, 6 };
    lookup[0xDE] = { &m6502_p::DEC, &m6502_p::ABX, 7 };
    lookup[0xCA] = { &m6502_p::DEX, &m6502_p::IMP, 2 };
    lookup[0x88] = { &m6502_p::DEY, &m6502_p::IMP, 2 };

    // COMPARE (CMP, CPX, CPY)
    lookup[0xC9] = { &m6502_p::CMP, &m6502_p::IMM, 2 };
    lookup[0xC5] = { &m6502_p::CMP, &m6502_p::ZP0, 3 };
    lookup[0xD5] = { &m6502_p::CMP, &m6502_p::ZPX, 4 };
    lookup[0xCD] = { &m6502_p::CMP, &m6502_p::ABS, 4 };
    lookup[0xDD] = { &m6502_p::CMP, &m6502_p::ABX, 4 }; // +1 page cross
    lookup[0xD9] = { &m6502_p::CMP, &m6502_p::ABY, 4 }; // +1 page cross
    lookup[0xC1] = { &m6502_p::CMP, &m6502_p::IZX, 6 };
    lookup[0xD1] = { &m6502_p::CMP, &m6502_p::IZY, 5 }; // +1 page cross
    lookup[0xD2] = { &m6502_p::CMP, &m6502_p::ZPI, 5 };

    lookup[0xE0] = { &m6502_p::CPX, &m6502_p::IMM, 2 };
    lookup[0xE4] = { &m6502_p::CPX, &m6502_p::ZP0, 3 };
    lookup[0xEC] = { &m6502_p::CPX, &m6502_p::ABS, 4 };

    lookup[0xC0] = { &m6502_p::CPY, &m6502_p::IMM, 2 };
    lookup[0xC4] = { &m6502_p::CPY, &m6502_p::ZP0, 3 };
    lookup[0xCC] = { &m6502_p::CPY, &m6502_p::ABS, 4 };

    // BRANCHES
    lookup[0x90] = { &m6502_p::BCC, &m6502_p::REL, 2 }; // +1 taken, +1 page cross
    lookup[0xB0] = { &m6502_p::BCS, &m6502_p::REL, 2 };
    lookup[0xF0] = { &m6502_p::BEQ, &m6502_p::REL, 2 };
    lookup[0xD0] = { &m6502_p::BNE, &m6502_p::REL, 2 };
    lookup[0x10] = { &m6502_p::BPL, &m6502_p::REL, 2 };
    lookup[0x30] = { &m6502_p::BMI, &m6502_p::REL, 2 };
    lookup[0x50] = { &m6502_p::BVC, &m6502_p::REL, 2 };
    lookup[0x70] = { &m6502_p::BVS, &m6502_p::REL, 2 };
    lookup[0x80] = { &m6502_p::BRA, &m6502_p::REL, 2 }; // +1 taken (always)

    // JUMPS & CALLS
    lookup[0x4C] = { &m6502_p::JMP, &m6502_p::ABS, 3 };
    lookup[0x6C] = { &m6502_p::JMP, &m6502_p::IND, 6 };
    lookup[0x7C] = { &m6502_p::JMP, &m6502_p::IAX, 6 };
    lookup[0x20] = { &m6502_p::JSR, &m6502_p::ABS, 6 };
    lookup[0x60] = { &m6502_p::RTS, &m6502_p::IMP, 6 };
    lookup[0x00] = { &m6502_p::BRK, &m6502_p::IMP, 7 };
    lookup[0x40] = { &m6502_p::RTI, &m6502_p::IMP, 6 };

    // SYSTEM / FLAGS
    lookup[0x18] = { &m6502_p::CLC, &m6502_p::IMP, 2 };
    lookup[0x38] = { &m6502_p::SEC, &m6502_p::IMP, 2 };
    lookup[0x58] = { &m6502_p::CLI, &m6502_p::IMP, 2 };
    lookup[0x78] = { &m6502_p::SEI, &m6502_p::IMP, 2 };
    lookup[0xB8] = { &m6502_p::CLV, &m6502_p::IMP, 2 };
    lookup[0xD8] = { &m6502_p::CLD, &m6502_p::IMP, 2 };
    lookup[0xF8] = { &m6502_p::SED, &m6502_p::IMP, 2 };
    lookup[0xEA] = { &m6502_p::NOP, &m6502_p::IMP, 2 };
}

// ============================================================================
//  Device Lifecycle
// ============================================================================
void m6502_p::device_start() {
    std::cout << "[W65C02S] Initialized." << std::endl;
}

void m6502_p::device_reset() {
    A = 0; X = 0; Y = 0;
    S = 0xFD;
    P = 0x34; // IRQ Disabled, Break=0, Unused=1
    
    // Read Reset Vector
    u16 lo = read_byte(0xFFFC);
    u16 hi = read_byte(0xFFFD);
    PC = (hi << 8) | lo;

    m_icount = 0;
    m_reset_line = false;
    m_rdy_line = true; // Default to Ready
    
    std::cout << "[W65C02S] Reset. PC: " << std::hex << PC << std::dec << std::endl;
}

void m6502_p::set_input_line(int line, bool state) {
    switch(line) {
        case IRQ_LINE: m_irq_line = state; break;
        case NMI_LINE: m_nmi_line = state; break;
        case RESET_LINE:
            if (state && !m_reset_line) {
                // Rising edge of Reset assertion? Or Level?
                // Real 6502 resets when line is LOW. We assume active HIGH logic here for simplicity.
                // If state=true (Asserted), we hold reset.
            }
            m_reset_line = state;
            break;
        case RDY_LINE: m_rdy_line = state; break;
    }
}

void m6502_p::memory_map(address_map &map) { 
    (void)map;
}

// ============================================================================
//  Execute Cycle
// ============================================================================
void m6502_p::execute_run() {
    
    // If Reset is held, do nothing
    if (m_reset_line) {
        m_icount = 0;
        return;
    }

    while (m_icount > 0) {
        // RDY Line Logic: If RDY is Low, CPU halts (simulated by not running)
        if (!m_rdy_line) {
            // We just burn cycles waiting
            m_icount = 0; 
            break;
        }

        // Interrupts
        if (m_nmi_line && !m_nmi_prev) {
            nmi();
            m_nmi_prev = m_nmi_line;
            continue;
        }
        m_nmi_prev = m_nmi_line;

        if (m_irq_line && get_flag(I) == 0) {
            irq();
            continue;
        }

        // Execute Instruction
        m_cycles = 0;
        opcode = read_byte(PC++);

        // We log the Program Counter (PC-1 because it was just incremented) 
        // and the hex value of the opcode.
        if (DebugView::m_en_cpu_trace) {
            DebugView::add_log(LOG_CPU, "[$%04X] EXEC: %02X", (PC - 1), opcode);
        }
        
        m_cycles = lookup[opcode].cycles;
        u8 extra1 = (this->*lookup[opcode].addrmode)();
        (this->*lookup[opcode].operate)();
        m_cycles += extra1;

        m_icount -= m_cycles;
        m_total_cycles += m_cycles;
    }
}

// ============================================================================
//  Interrupts
// ============================================================================
void m6502_p::irq() {
    // Saving the program counter
    // The cpu must know where to return after teh interrupt is finished.
    // Here we push the 16-bit program counter to the stack (High-byte, then low-byte)
    push_word(PC);

    // Flag manipulation
    // B Flag (break): Must be 0 for IRQ's.
    // U Flag (unused): This is always set to 1
    set_flag(B, 0); set_flag(U, 1);
    
    // Save the status Register (P)
    // This will save the state of the flags 
    // so they can be restored exactly as they were previously.
    push_byte(P);

    // This was originally place above the push_byte(P) Flag
    // which is a bug, moved to after the push so that the interrupt disable
    // does not affect the flags. 
    set_flag(I, 1);

    // Fetch the Interrupt Vector
    // The 6502 has a "hardcoded" address where it looks for the IRQ handler.
    // It reads the 16-bit address stored at 0xFFFE (Low) and 0xFFFF (High).The 6502 has a "hardcoded" address where it looks for the IRQ handler.
    u16 lo = read_byte(0xFFFE);
    u16 hi = read_byte(0xFFFF);

    // Jump to the handler
    // Set the program counter to the address we just read
    PC = (hi << 8) | lo;
    
    // Accounting for the time
    // An IRQ sequenct always takes exactly 7 clock cycles.
    m_cycles = 7;               // This instruction too 7 cycles
    m_icount -= 7;              // Subtract from the remaining time slice
    m_total_cycles += 7;        // Add to total system uptime
}

void m6502_p::nmi() {
    push_word(PC);
    set_flag(B, 0); set_flag(U, 1); set_flag(I, 1);
    push_byte(P);
    
    u16 lo = read_byte(0xFFFA);
    u16 hi = read_byte(0xFFFB);
    PC = (hi << 8) | lo;
    
    m_cycles = 7;
    m_icount -= 7;
    m_total_cycles += 7;
}

// ============================================================================
//  Helpers
// ============================================================================
void m6502_p::set_flag(Flags f, bool v) {
    if (v) P |= f; else P &= ~f;
}

u8 m6502_p::get_flag(Flags f) {
    return (P & f) ? 1 : 0;
}

u8 m6502_p::fetch_data() {
    if (lookup[opcode].addrmode != &m6502_p::IMP) {
        fetched = read_byte(addr_abs);
    } else {
        fetched = A;
    }
    return fetched;
}

void m6502_p::push_byte(u8 v) { write_byte(0x0100 + S--, v); }
u8 m6502_p::pop_byte() { return read_byte(0x0100 + ++S); }

void m6502_p::push_word(u16 v) {
    push_byte((v >> 8) & 0xFF);
    push_byte(v & 0xFF);
}
u16 m6502_p::pop_word() {
    u16 lo = pop_byte();
    u16 hi = pop_byte();
    return (hi << 8) | lo;
}

// ============================================================================
//  Addressing Modes
// ============================================================================
u8 m6502_p::IMP() { fetched = A; return 0; }
u8 m6502_p::IMM() { addr_abs = PC++; return 0; }
u8 m6502_p::ZP0() { addr_abs = read_byte(PC++) & 0x00FF; return 0; }
u8 m6502_p::ZPX() { addr_abs = (read_byte(PC++) + X) & 0x00FF; return 0; }
u8 m6502_p::ZPY() { addr_abs = (read_byte(PC++) + Y) & 0x00FF; return 0; }
u8 m6502_p::ABS() { 
    u16 lo = read_byte(PC++); 
    u16 hi = read_byte(PC++); 
    addr_abs = (hi << 8) | lo; 
    return 0; 
}
u8 m6502_p::ABX() {
    u16 lo = read_byte(PC++);
    u16 hi = read_byte(PC++);
    u16 base = (hi << 8) | lo;
    addr_abs = base + X;
    return ((addr_abs & 0xFF00) != (base & 0xFF00)) ? 1 : 0;
}
u8 m6502_p::ABY() {
    u16 lo = read_byte(PC++);
    u16 hi = read_byte(PC++);
    u16 base = (hi << 8) | lo;
    addr_abs = base + Y;
    return ((addr_abs & 0xFF00) != (base & 0xFF00)) ? 1 : 0;
}
u8 m6502_p::IND() {
    // W65C02S FIX: Page Boundary Bug DOES NOT exist here.
    u16 ptr_lo = read_byte(PC++);
    u16 ptr_hi = read_byte(PC++);
    u16 ptr = (ptr_hi << 8) | ptr_lo;
    
    // Correctly read across page boundaries
    addr_abs = (read_byte(ptr + 1) << 8) | read_byte(ptr);
    return 0;
}
u8 m6502_p::IZX() {
    u16 t = read_byte(PC++);
    u16 lo = read_byte((u16)(t + X) & 0x00FF);
    u16 hi = read_byte((u16)(t + X + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    return 0;
}
u8 m6502_p::IZY() {
    u16 t = read_byte(PC++);
    u16 lo = read_byte(t & 0x00FF);
    u16 hi = read_byte((t + 1) & 0x00FF);
    u16 base = (hi << 8) | lo;
    addr_abs = base + Y;
    return ((addr_abs & 0xFF00) != (base & 0xFF00)) ? 1 : 0;
}
u8 m6502_p::ZPI() {
    u16 t = read_byte(PC++);
    u16 lo = read_byte(t & 0x00FF);
    u16 hi = read_byte((t + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    return 0;
}
u8 m6502_p::IAX() {
    u16 lo = read_byte(PC++);
    u16 hi = read_byte(PC++);
    u16 base = (hi << 8) | lo;
    u16 ptr = base + X;
    u16 p_lo = read_byte(ptr);
    u16 p_hi = read_byte(ptr + 1);
    addr_abs = (p_hi << 8) | p_lo;
    return 0;
}
u8 m6502_p::REL() {
    addr_rel = read_byte(PC++);
    if (addr_rel & 0x80) addr_rel |= 0xFF00; // Sign extend
    return 0;
}

// ============================================================================
//  Instructions
// ============================================================================
void m6502_p::XXX() {}
void m6502_p::NOP() {}

void m6502_p::LDA() { fetch_data(); A = fetched; set_flag(Z, A==0); set_flag(N, A&0x80); }
void m6502_p::LDX() { fetch_data(); X = fetched; set_flag(Z, X==0); set_flag(N, X&0x80); }
void m6502_p::LDY() { fetch_data(); Y = fetched; set_flag(Z, Y==0); set_flag(N, Y&0x80); }
void m6502_p::STA() { write_byte(addr_abs, A); }
void m6502_p::STX() { write_byte(addr_abs, X); }
void m6502_p::STY() { write_byte(addr_abs, Y); }

// W65C02S: STZ (Store Zero)
void m6502_p::STZ() { write_byte(addr_abs, 0x00); }

void m6502_p::TAX() { X = A; set_flag(Z, X==0); set_flag(N, X&0x80); }
void m6502_p::TAY() { Y = A; set_flag(Z, Y==0); set_flag(N, Y&0x80); }
void m6502_p::TXA() { A = X; set_flag(Z, A==0); set_flag(N, A&0x80); }
void m6502_p::TYA() { A = Y; set_flag(Z, A==0); set_flag(N, A&0x80); }
void m6502_p::TSX() { X = S; set_flag(Z, X==0); set_flag(N, X&0x80); }
void m6502_p::TXS() { S = X; }

void m6502_p::PHA() { push_byte(A); }
void m6502_p::PLA() { A = pop_byte(); set_flag(Z, A==0); set_flag(N, A&0x80); }
void m6502_p::PHP() { push_byte(P | B | U); }
void m6502_p::PLP() { P = pop_byte(); set_flag(U, 1); }

// W65C02S Stack Extensions
void m6502_p::PHX() { push_byte(X); }
void m6502_p::PLX() { X = pop_byte(); set_flag(Z, X==0); set_flag(N, X&0x80); }
void m6502_p::PHY() { push_byte(Y); }
void m6502_p::PLY() { Y = pop_byte(); set_flag(Z, Y==0); set_flag(N, Y&0x80); }

void m6502_p::INC() {
    // Check if Accumulator mode (Implied) - W65C02S feature
    if (lookup[opcode].addrmode == &m6502_p::IMP) {
        A++;
        set_flag(Z, A==0); set_flag(N, A&0x80);
    } else {
        fetch_data();
        u16 t = fetched + 1;
        write_byte(addr_abs, t & 0xFF);
        set_flag(Z, (t&0xFF)==0); set_flag(N, t&0x80);
    }
}
void m6502_p::DEC() {
    if (lookup[opcode].addrmode == &m6502_p::IMP) {
        A--;
        set_flag(Z, A==0); set_flag(N, A&0x80);
    } else {
        fetch_data();
        u16 t = fetched - 1;
        write_byte(addr_abs, t & 0xFF);
        set_flag(Z, (t&0xFF)==0); set_flag(N, t&0x80);
    }
}
void m6502_p::INX() { X++; set_flag(Z, X==0); set_flag(N, X&0x80); }
void m6502_p::DEX() { X--; set_flag(Z, X==0); set_flag(N, X&0x80); }
void m6502_p::INY() { Y++; set_flag(Z, Y==0); set_flag(N, Y&0x80); }
void m6502_p::DEY() { Y--; set_flag(Z, Y==0); set_flag(N, Y&0x80); }

void m6502_p::ASL() {
    fetch_data();
    u16 t = (u16)fetched << 1;
    set_flag(C, (t & 0xFF00) > 0);
    set_flag(Z, (t & 0xFF) == 0);
    set_flag(N, t & 0x80);
    if (lookup[opcode].addrmode == &m6502_p::IMP) A = t & 0xFF;
    else write_byte(addr_abs, t & 0xFF);
}
void m6502_p::LSR() {
    fetch_data();
    set_flag(C, fetched & 1);
    u16 t = fetched >> 1;
    set_flag(Z, (t & 0xFF) == 0);
    set_flag(N, t & 0x80);
    if (lookup[opcode].addrmode == &m6502_p::IMP) A = t & 0xFF;
    else write_byte(addr_abs, t & 0xFF);
}
void m6502_p::ROL() {
    fetch_data();
    u16 t = (fetched << 1) | get_flag(C);
    set_flag(C, (t & 0xFF00) > 0);
    set_flag(Z, (t & 0xFF) == 0);
    set_flag(N, t & 0x80);
    if (lookup[opcode].addrmode == &m6502_p::IMP) A = t & 0xFF;
    else write_byte(addr_abs, t & 0xFF);
}
void m6502_p::ROR() {
    fetch_data();
    u16 t = (fetched >> 1) | (get_flag(C) << 7);
    set_flag(C, fetched & 1);
    set_flag(Z, (t & 0xFF) == 0);
    set_flag(N, t & 0x80);
    if (lookup[opcode].addrmode == &m6502_p::IMP) A = t & 0xFF;
    else write_byte(addr_abs, t & 0xFF);
}

void m6502_p::AND() { fetch_data(); A &= fetched; set_flag(Z, A==0); set_flag(N, A&0x80); }
void m6502_p::ORA() { fetch_data(); A |= fetched; set_flag(Z, A==0); set_flag(N, A&0x80); }
void m6502_p::EOR() { fetch_data(); A ^= fetched; set_flag(Z, A==0); set_flag(N, A&0x80); }

void m6502_p::BIT() {
    fetch_data();
    u8 t = A & fetched;
    set_flag(Z, t == 0);
    set_flag(N, fetched & (1<<7));
    set_flag(V, fetched & (1<<6));
}

// W65C02S: TRB (Test and Reset Bits)
// Z = (A & M) == 0. Then M = M & ~A.
void m6502_p::TRB() {
    fetch_data();
    set_flag(Z, (A & fetched) == 0);
    write_byte(addr_abs, fetched & ~A);
}

// W65C02S: TSB (Test and Set Bits)
// Z = (A & M) == 0. Then M = M | A.
void m6502_p::TSB() {
    fetch_data();
    set_flag(Z, (A & fetched) == 0);
    write_byte(addr_abs, fetched | A);
}

void m6502_p::ADC() {
    fetch_data();
    u16 t = (u16)A + (u16)fetched + (u16)get_flag(C);
    set_flag(C, t > 255);
    set_flag(Z, (t & 0xFF) == 0);
    set_flag(N, t & 0x80);
    set_flag(V, (~((u16)A ^ (u16)fetched) & ((u16)A ^ t)) & 0x0080);
    A = t & 0xFF;
}
void m6502_p::SBC() {
    fetch_data();
    u16 val = ((u16)fetched) ^ 0x00FF;
    u16 t = (u16)A + val + (u16)get_flag(C);
    set_flag(C, t > 255);
    set_flag(Z, (t & 0xFF) == 0);
    set_flag(N, t & 0x80);
    set_flag(V, (~((u16)A ^ val) & ((u16)A ^ t)) & 0x0080);
    A = t & 0xFF;
}

void m6502_p::CMP() { fetch_data(); u16 t = (u16)A - (u16)fetched; set_flag(C, A>=fetched); set_flag(Z, (t&0xFF)==0); set_flag(N, t&0x80); }
void m6502_p::CPX() { fetch_data(); u16 t = (u16)X - (u16)fetched; set_flag(C, X>=fetched); set_flag(Z, (t&0xFF)==0); set_flag(N, t&0x80); }
void m6502_p::CPY() { fetch_data(); u16 t = (u16)Y - (u16)fetched; set_flag(C, Y>=fetched); set_flag(Z, (t&0xFF)==0); set_flag(N, t&0x80); }

void m6502_p::CLC() { set_flag(C, 0); }
void m6502_p::SEC() { set_flag(C, 1); }
void m6502_p::CLI() { set_flag(I, 0); }
void m6502_p::SEI() { set_flag(I, 1); }
void m6502_p::CLV() { set_flag(V, 0); }
void m6502_p::CLD() { set_flag(D, 0); }
void m6502_p::SED() { set_flag(D, 1); }

void m6502_p::JMP() { PC = addr_abs; }
void m6502_p::JSR() { push_word(PC - 1); PC = addr_abs; }
void m6502_p::RTS() { PC = pop_word() + 1; }
void m6502_p::BRK() {
    PC++; 
    set_flag(I, 1); 
    push_word(PC); 
    set_flag(B, 1); 
    push_byte(P | B | U); 
    set_flag(B, 0); 
    u16 lo = read_byte(0xFFFE); 
    u16 hi = read_byte(0xFFFF); 
    PC = (hi << 8) | lo;
}
void m6502_p::RTI() { 
    P = pop_byte(); set_flag(U, 1); set_flag(B, 0); 
    PC = pop_word(); 
}

void m6502_p::branch_exec(bool cond) {
    if (!cond) return;
    m_cycles++;
    addr_abs = PC + addr_rel;
    if ((addr_abs & 0xFF00) != (PC & 0xFF00)) m_cycles++;
    PC = addr_abs;
}

void m6502_p::BCC() { branch_exec(get_flag(C) == 0); }
void m6502_p::BCS() { branch_exec(get_flag(C) == 1); }
void m6502_p::BEQ() { branch_exec(get_flag(Z) == 1); }
void m6502_p::BNE() { branch_exec(get_flag(Z) == 0); }
void m6502_p::BMI() { branch_exec(get_flag(N) == 1); }
void m6502_p::BPL() { branch_exec(get_flag(N) == 0); }
void m6502_p::BVC() { branch_exec(get_flag(V) == 0); }
void m6502_p::BVS() { branch_exec(get_flag(V) == 1); }
void m6502_p::BRA() { branch_exec(true); }