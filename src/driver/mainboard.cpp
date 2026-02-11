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

    // Default to Schematic 1
    set_machine_type(MachineType::SCHEMATIC_1_BASIC);

    m_cpu->device_start();
}

// ============================================================================
//  Hardware Configuration Switcher
// ============================================================================
void mb_driver::set_machine_type(MachineType type) {
    m_current_type = type;
    
    // 1. Reset Internal State
    m_last_e_state = false;
    m_port_b_data = 0;
    
    // 2. Re-Install Memory Map
    // We create a new map and force the CPU to use it.
    // (Note: In a real MAME system, the manager handles this rebuild.
    //  Here, we just re-run the map setup logic).
    static address_map active_map;
    map_setup(active_map);
    m_cpu->install_map(&active_map);

    // 3. Re-Wire Interrupts & I/O based on Schematic
    
    // --- COMMON INTERRUPT LOGIC ---
    auto irq_handler = [this](bool state) {
        m_cpu->set_input_line(m6502_p::IRQ_LINE, state ? 1 : 0);
    };
    m_via.set_irq_callback(irq_handler);
    m_acia.set_irq_callback(irq_handler);


    // --- SCHEMATIC SPECIFIC WIRING ---
    if (m_current_type == MachineType::SCHEMATIC_1_BASIC) {
        std::cout << "[Board] Configured for Schematic 1 (Basic)" << std::endl;
        
        // SCHEMATIC 1: LCD on Port B (Data) + Port A (Control)
        // PB0-7 = Data Bus
        // PA5=RS, PA6=RW, PA7=E
        
        m_via.set_port_b_callback([this](u8 data) {
            m_port_b_data = data;
        });
        
        m_via.set_port_a_callback([this](u8 data) {
            bool rs = (data & 0x20); // Bit 5
            bool rw = (data & 0x40); // Bit 6
            bool e  = (data & 0x80); // Bit 7
            
            if (m_last_e_state && !e) { // Falling Edge
                // 8-bit write using Port B data
                m_lcd.write_8bit(m_port_b_data, rs, rw);
            }
            m_last_e_state = e;
        });
    }
    else if (m_current_type == MachineType::SCHEMATIC_2_SERIAL) {
        std::cout << "[Board] Configured for Schematic 2 (Serial)" << std::endl;
        
        // SCHEMATIC 2: 
        // (You will implement the specific wiring here later based on the 2nd schematic image)
        // For now, leave it blank or default to Basic behavior.
    }

    // 4. Clear Lines
    m_cpu->set_input_line(m6502_p::IRQ_LINE, 0); 
    m_cpu->set_input_line(m6502_p::NMI_LINE, 0);
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

    m_via.clock();

    // Run the CPU
    m_cpu->execute_run();

}

// ============================================================================
//  The Memory Map (74HC00 Logic)
// ============================================================================
void mb_driver::map_setup(address_map& map) {
    
     auto read_logic = [this](u16 addr) -> u8 {
        // Common ROM
        if (addr >= 0x8000) return m_rom.read(addr - 0x8000);
        
        // I/O Mapping Changes based on Schematic!
        if (m_current_type == MachineType::SCHEMATIC_1_BASIC) {
            // Basic: VIA at $6000
            if (addr >= 0x6000 && addr <= 0x7FFF) return m_via.read(addr - 0x6000);
        }
        else {
            // Serial: ACIA usually at $5000, VIA at $6000
            if (addr >= 0x6000 && addr <= 0x7FFF) return m_via.read(addr - 0x6000);
            if (addr >= 0x4000 && addr <= 0x5FFF) return m_acia.read(addr - 0x4000); 
        }

        // Common RAM
        if (addr < 0x4000) return m_ram.read(addr);
        
        return 0xEA; // Open Bus
    };

    auto write_logic = [this](u16 addr, u8 data) {
        if (addr >= 0x8000)      m_rom.write(addr - 0x8000, data);
        else if (addr < 0x4000)  m_ram.write(addr, data);
        else {
            // I/O Write Logic
            if (m_current_type == MachineType::SCHEMATIC_1_BASIC) {
                if (addr >= 0x6000) m_via.write(addr - 0x6000, data);
            }
            else {
                if (addr >= 0x6000) m_via.write(addr - 0x6000, data);
                else if (addr >= 0x4000) m_acia.write(addr - 0x4000, data);
            }
        }
    };

    map.install(0x0000, 0xFFFF, read_logic, write_logic);
    
    // Debug Handler (Safe Access)
    map.install_debug_handler(0x0000, 0xFFFF, [this](u16 addr) -> u8 {
        if (addr >= 0x8000) return m_rom.read(addr - 0x8000);
        if (addr < 0x4000)  return m_ram.read(addr);
        
        // I/O Debug
        if (addr >= 0x6000 && addr <= 0x7FFF) return m_via.peek(addr - 0x6000);
        if (m_current_type == MachineType::SCHEMATIC_2_SERIAL) {
            if (addr >= 0x4000 && addr <= 0x5FFF) return m_acia.read(addr - 0x4000);
        }
        return 0x00;
    });
}
