// ============================================================================
// Copyright (c) 2026 Andrew Young
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
// 
// ============================================================================

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
    
    // Wiring in the MAX232 directly to the ACIA
    m_serial_port = new Serial_Port(&m_acia);
}

mb_driver::~mb_driver() {
    delete m_cpu;
    delete m_serial_port;
}

m6502_p* mb_driver::get_cpu() {
    return m_cpu;
}

void mb_driver::init() {
    std::cout << "[Board] Powering on..." << std::endl;

    // 1. Load Firmware
    // (Ensure you have a 'rom.bin' or this stays 0xFF)
    //if (!m_rom.load_from_file("rom.bin")) {
    //    std::cerr << "[Board] Warning: rom.bin not found. ROM is empty." << std::endl;
    //}

    // Default to Schematic 1
    set_machine_type(MachineType::SCHEMATIC_1_BASIC);

    m_cpu->device_start();
}

// ============================================================================
//  Hardware Configuration Switcher
// ============================================================================
void mb_driver::set_machine_type(MachineType type) {
    m_current_type = type;
    
    std::cout << "[Driver] Switching hardware schematic..." << std::endl;

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
    //auto irq_handler = [this](bool state) {
    //    m_cpu->set_input_line(m6502_p::IRQ_LINE, state ? 1 : 0);
    //};
    m_via.set_irq_callback([this](bool state) {
        m_via_irq_active = state;
        resolve_cpu_irq();
    });
    m_acia.set_irq_callback([this](bool state) {
        m_acia_irq_active = state;
        resolve_cpu_irq();
    });

    //m_via.set_irq_callback(irq_handler);
    //m_acia.set_irq_callback(irq_handler); // Wires the ACIA Pin 26 to CPU pin 4


    // --- SCHEMATIC SPECIFIC WIRING ---
    if (m_current_type == MachineType::SCHEMATIC_1_BASIC) {
        if(!load_rom("rom.bin")){
             std::cerr << "[Warning] Could not find default rom.bin for Basic Schematic." << std::endl;
        }
       
        // DISCONNECT SERIAL when switching back to the BASIC Schematic
        if (m_serial_port->is_connected()){
            m_serial_port->disconnect();
        }
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
        if (!load_rom("rs232.bin")){
            std::cerr << "[Warning] Could not find default wozmon.bin for Serial Schematic." << std::endl;
        }
        
        m_serial_port->connect("COM5", 9600);
        
        // SCHEMATIC 2: LCD in 4-bit mode entirely on Port B
        // PB0-PB3 = Data Nibble
        // PB4 = RS, PB5 = R/~W, PB6 = E

        m_via.set_port_b_callback([this](u8 data) {
            // Mask out the control bits to get just the 4-bit data (PB0-PB3)
            m_port_b_data = (u8)((data & 0x0F) << 4);

            // Extracting the Control Pins
            bool rs = (data & 0x10) != 0; // Bit 4
            bool rw = (data & 0x20) != 0; // Bit 5
            bool e  = (data & 0x40) != 0; // Bit 6

            // The HD44780 LCD reads data exactly when the Enable pin goes from HIGH to LOW.
            //if (m_last_e_state && !e) {
                // The LCD class takes (data, rs, rw) like the 8-bit version does
                m_lcd.write_4bit(m_port_b_data, rs, rw, e);
            //}
           // m_last_e_state = e;
            
        });
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
    m_acia.reset();

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

    // Drain the ACIA's transmit buffer to the physical COM Port
    if (m_serial_port && m_current_type == MachineType::SCHEMATIC_2_SERIAL) {
        m_serial_port->update();
    }

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

        // Ben brings up a situation that we do not what to happen in the hardware.
        // Because the ACIA is using A15-A12, with the bits being as follows
        // A15 = 0, A14 = 1, A13 = 0, A12 = 1 
        // And the VIA is also using these Address line where the bits are
        // A15 = 0, A14 = 1, A13 = 1, A12 = 0
        // We don't want A15 to A12 to enable both chips at the same time.
        // So I'm going to add a safety net to catch those and return an open bus condition
        if (addr >= 0x7000 && addr <= 0x7FFF) {
            return 0xEA;    // NOP or Open Bus condition.
        }
        
        // I/O Mapping Changes based on Schematic!
        if (m_current_type == MachineType::SCHEMATIC_1_BASIC) {
            // Basic: VIA at $6000
            if (addr >= 0x6000 && addr <= 0x7FFF) return m_via.read(addr & 0x0F);//addr - 0x6000
        }
        else {
            // Serial: ACIA usually at $5000, VIA at $6000
            if (addr >= 0x6000 && addr <= 0x7FFF) return m_via.read(addr & 0x0F);//addr - 0x6000
            if (addr >= 0x5000 && addr <= 0x5FFF) return m_acia.read(addr & 0x03); //addr - 0x4000
        }

        // Common RAM
        if (addr < 0x4000) return m_ram.read(addr);
        
        return 0xEA; // Open Bus
    };

    auto write_logic = [this](u16 addr, u8 data) {
        if (addr >= 0x8000)      m_rom.write(addr - 0x8000, data);
        else if (addr < 0x4000)  m_ram.write(addr, data);
        else {
            // Also need to create the safety net for write condition for 0x7000 to 0x7FFF Addresses.
            if (addr >= 0x7000 && addr <= 0x7FFF){
                return; // We will do nothing. 
            }

            // I/O Write Logic
            if (m_current_type == MachineType::SCHEMATIC_1_BASIC) {
                if (addr >= 0x6000 && addr <= 0x7FFF) m_via.write(addr & 0x0F, data); //addr - 0x6000
            }
            else {
                if (addr >= 0x6000 && addr <= 0x7FFF) m_via.write(addr & 0x0F, data);//addr - 0x6000
                else if (addr >= 0x5000 && addr <= 0x5FFF) m_acia.write(addr & 0x03, data);//addr - 0x4000
            }
        }
    };

    map.install(0x0000, 0xFFFF, read_logic, write_logic);
    
    // Debug Handler (Safe Access)
    map.install_debug_handler(0x0000, 0xFFFF, [this](u16 addr) -> u8 {
        if (addr >= 0x8000) return m_rom.read(addr - 0x8000);
        if (addr < 0x4000)  return m_ram.read(addr);

        // Safety Net for Addresses from 0x7000 to 0x7FFF
        if (addr >= 0x7000 && addr >= 0x7FFF) return 0x00;
        
        // I/O Debug
        if (addr >= 0x6000 && addr <= 0x7FFF) return m_via.peek(addr & 0x0F);// addr - 0x6000
        if (m_current_type == MachineType::SCHEMATIC_2_SERIAL) {
            if (addr >= 0x5000 && addr <= 0x5FFF) return m_acia.peek(addr & 0x03);//addr - 0x4000
        }
        return 0x00;
    });
}

void mb_driver::resolve_cpu_irq() {
    // if either the via or the acia is asseting an interrupt
    if (m_via_irq_active || m_acia_irq_active){
        m_cpu->set_input_line(m6502_p::IRQ_LINE, 1);
    } else {
        m_cpu->set_input_line(m6502_p::IRQ_LINE, 0);
    }
}