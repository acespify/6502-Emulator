#pragma once
#include "../emu/machine.h"
#include "../devices/cpu/m6502.h"
#include "../devices/memory/28c256.h"
#include "../devices/memory/62256.h"
#include "../devices/io/w65c22.h"
#include "../devices/io/w65c51.h"
#include "../devices/video/nhd_0216k1z.h"
#include "../devices/logic/74hc00.h"

// ============================================================================
// Hardware variants
// ============================================================================
enum class MachineType {
    SCHEMATIC_1_BASIC,  // LCD ON PORT B (8-BIT), NO Serial
    SCHEMATIC_2_SERIAL  // LCD
};

// ============================================================================
//  Driver: Ben Eater 6502 Computer
// ============================================================================
class mb_driver : public machine {
public:
    mb_driver();
    virtual ~mb_driver();

    void init() override;
    void reset() override;
    void run(int cycles) override;

    // Method to switch Hardware
    void set_machine_type(MachineType type);
    MachineType get_machine_type() const { return m_current_type; }

    // Expose components for UI
    m6502_p* get_cpu() override;
    w65c22* get_via() override { return &m_via; }
    w65c51* get_acia() override { return &m_acia; }
    nhd_0216k1z* get_lcd() { return &m_lcd; }

    bool load_rom(const char* filename) {
        std::cout << "[Driver] Attempting to load ROM from: " << filename << std::endl;
        bool result = m_rom.load_from_file(filename);
        if (result) {
            std::cout << "[Driver] Success! ROM loaded." << std::endl;
        } else {
            std::cerr << "[Driver] Failed to load ROM." << std::endl;
        }
        return result;
    }

private:
    MachineType m_current_type = MachineType::SCHEMATIC_1_BASIC;

    // --- The Chips ---
    // We use a custom CPU subclass internally to bind the map
    class board_cpu; 
    board_cpu* m_cpu;
    
    eeprom_28c256 m_rom; // U2
    ram_62256     m_ram; // U6
    w65c22        m_via; // U5 
    w65c51        m_acia; // U7 (ACIA)
    nhd_0216k1z   m_lcd; // U3 (LCD)

    u8   m_port_b_data = 0x00;
    bool m_last_e_state = false;    // To detect the edge of the Enable pin

    // --- Wiring Logic ---
    void map_setup(class address_map& map);
};