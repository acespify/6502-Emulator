#pragma once
#include "../emu/machine.h"
#include "../devices/cpu/m6502.h"
#include "../devices/memory/28c256.h"
#include "../devices/memory/62256.h"
#include "../devices/io/w65c22.h"
#include "../devices/io/w65c51.h"
#include "../devices/video/nhd_0216k1z.h"

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

    // Expose components for UI
    m6502_p* get_cpu() override;
    w65c22* get_via() override { return &m_via; }
    w65c51* get_acia() override { return &m_acia; }
    nhd_0216k1z* get_lcd() { return &m_lcd; }

    bool load_rom(const char* filename) {
        return m_rom.load_from_file(filename);
    }

private:
    // --- The Chips ---
    // We use a custom CPU subclass internally to bind the map
    class board_cpu; 
    board_cpu* m_cpu;
    
    eeprom_28c256 m_rom; // U2
    ram_62256     m_ram; // U6
    w65c22        m_via; // U5 
    w65c51        m_acia; // U7 (ACIA)
    nhd_0216k1z   m_lcd; // U3 (LCD)

    bool m_last_e_state = false;

    // --- Wiring Logic ---
    void map_setup(class address_map& map);
};