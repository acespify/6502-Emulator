#include "mainboard.h"
#include "../emu/map.h"
#include <iostream>


// ============================================================================
//  Custom CPU wrapper
// ============================================================================
//  WHY: The m6502_p class calls 'memory_map' virtually. We need to intercept 
//       that call and redirect it to the driver's wiring logic.
// ============================================================================
class mb_driver::board_cpu : public m6502_p {
    public:
        mb_driver* driver;  // Pointer back to the motherboard
        machine_config m_config;

        board_cpu(mb_driver* d)
            : m6502_p(m_config, "6502", nullptr, 1000000), driver(d) {}

        // Redirect the map setup to the driver
        void memory_map(address_map& map) override {
            driver->map_setup(map);
        }
        // helper to install the map pointer
        void install_map(address_map* map) {
            this->m_map = map;
        }
};

// ============================================================================
//  Driver Implementation
// ============================================================================

mb_driver::mb_driver() {
    // Create the CPU and connect it to this board
    m_cpu = new board_cpu(this);
}

mb_driver::~mb_driver() {
    delete m_cpu;
}

m6502_p* mb_driver::get_cpu() {
    return m_cpu;
}

void mb_driver::init() {
    std::cout << "[Board] Powering on..." << std::endl;

    // 1. Load Firmware
    // (Ensure you have a 'rom.bin' or this stays 0xFF)
    if (!m_rom.load_from_file("rom.bin")) {
        std::cerr << "[Board] Warning: rom.bin not found. ROM is empty." << std::endl;
    }

    // 2. Setup Address Map
    // We manually trigger the map setup on the CPU wrapper
    static address_map main_map;
    m_cpu->memory_map(main_map);
    m_cpu->install_map(&main_map); // (Hack: Assign protected member if accessible, or add setter)
    // Note: If m_map is protected, ensure board_cpu can assign it, or add 'set_map' to m6502_p.

    // --- FIX: Force Interrupt Lines Inactive ---
    // 0 = CLEAR_LINE (Inactive), 1 = ASSERT_LINE (Active)
    m_cpu->set_input_line(m6502_p::IRQ_LINE, 0); 
    m_cpu->set_input_line(m6502_p::NMI_LINE, 0);

    // ========================================================================
    // 3. WIRE THE SCHEMATIC (U5 <-> U3)
    // ========================================================================
    // U5 (VIA) Port B connects to U3 (LCD).
    // Schematic: PB0->RS, PB1->RW, PB2->E, PB4-7->DB4-7.
    
    m_via.set_port_b_callback([this](u8 data) {
        // Extract Control Lines (Lower 3 bits)
        bool rs = (data & 0x01); // PB0
        bool rw = (data & 0x02); // PB1
        bool e  = (data & 0x04); // PB2
        
        // Extract Data Nibble (Upper 4 bits)
        // The LCD expects the data on its DB4-DB7 lines.
        // Since we wired PB4-PB7 to DB4-DB7, the value is already aligned 
        // in the upper nibble of 'data'.
        u8 nibble = (data & 0xF0);
        
        m_lcd.write_4bit(nibble, rs, rw, e);
    });

    // U5 Port A is UNUSED (Schematic check: No connections to LCD)
    // m_via.set_port_a_callback(...) -> Removed.

    // ========================================================================
    // 4. WIRE INTERRUPTS
    // ========================================================================
    // Combine VIA and ACIA IRQs (Wire-OR logic).
    auto irq_handler = [this](bool state) {
        // Check if VIA OR ACIA is pulling IRQ low (Active Low)
        // Since set_input_line expects a boolean state (true/false),
        // we just pass the incoming state. 
        // In a real wire-or, we'd check if (via_irq || acia_irq).
        m_cpu->set_input_line(m6502_p::IRQ_LINE, state ? 1 : 0);
    };

    m_via.set_irq_callback(irq_handler);
    m_acia.set_irq_callback(irq_handler);

    m_cpu->device_start();
}

void mb_driver::reset() {
    std::cout << "[Board] Reset Sequence..." << std::endl;
    //m_rom.reset_memory(); // Optional, usually ROM doesn't reset
    m_ram.reset_memory();
    m_via.reset();

    // 2. Clear Interrupt Lines (Crucial Fix for "Stuck at 8000")
    // If these are floating or 1, the CPU gets stuck in an interrupt loop.
    m_cpu->set_input_line(m6502_p::IRQ_LINE, 0); 
    m_cpu->set_input_line(m6502_p::NMI_LINE, 0);

    m_cpu->device_reset();

    // If this stay 1 (Active), the CPU will sit at $8000 forever.
    //m_cpu->set_input_line(m6502_p::RESET_LINE, 0);
}

void mb_driver::run(int cycles) {
    // Give the CPU a budget of cycles
    m_cpu->icount_set(cycles);

    // Run the CPU
    m_cpu->execute_run();

    // Check how many cycles are LEFT
    int cycles_left = m_cpu->icount_get();

    printf("PC: %04X | SP: %02X | Opcode: %02X | Requested: %d | Left: %d\n", 
            m_cpu->get_pc(),
            m_cpu->get_sp(),
            m_cpu->debug_peek(m_cpu->get_pc()),
            cycles,
            cycles_left);
}

// ============================================================================
//  The Memory Map (74HC00 Logic)
// ============================================================================
void mb_driver::map_setup(address_map& map) {
    
    // --- Read Logic ---
    auto read_logic = [this](u16 addr) -> u8 {
        // 1. ROM (U2): Enabled when A15=1 ($8000-$FFFF)
        if (addr >= 0x8000) return m_rom.read(addr);

        // 2. VIA (U5): Enabled when A14=1 (and A15=0) ($6000-$7FFF)
        if (addr >= 0x6000) return m_via.read(addr);

        // 3. ACIA (U7): Enabled when A13=1? ($5000)
        // Standard Ben Eater map places ACIA at $5000 or overlapping $4000 range.
        if (addr >= 0x4000) return m_acia.read(addr);

        // 4. RAM (U6): Enabled when A14=0, A15=0 ($0000-$3FFF)
        return m_ram.read(addr);
    };

    // --- Write Logic ---
    auto write_logic = [this](u16 addr, u8 data) {
        if (addr >= 0x8000)      m_rom.write(addr -  0x8000, data);
        else if (addr >= 0x6000) m_via.write(addr - 0x6000, data);
        else if (addr >= 0x4000) m_acia.write(addr - 0x4000, data);
        else                     m_ram.write(addr, data);
    };

    map.install(0x0000, 0xFFFF, read_logic, write_logic);
    
    // Debug Handler (Safe Access)
    map.install_debug_handler(0x0000, 0xFFFF, [this](u16 addr) -> u8 {
        if (addr >= 0x8000) return m_rom.read(addr - 0x8000);
        if (addr >= 0x6000) return m_via.peek(addr - 0x6000);
        if (addr >= 0x4000) return m_acia.read(addr - 0x4000); // ACIA reads side-effect free usually
        return m_ram.read(addr);
    });
}