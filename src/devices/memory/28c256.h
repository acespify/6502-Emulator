#pragma once
#include "../../emu/di_memory.h"
#include <string>

// ============================================================================
//  Device: 28C256 (32K Parallel EEPROM)
// ============================================================================

class eeprom_28c256 : public device_memory_interface {
    public:
        // Constructor
        eeprom_28c256();
        virtual ~eeprom_28c256() = default;
    
        // Helper to clear memory (0xFF is the default state of erased EEPROM)
        void reset_memory();
        
        // --- INTERFACE ---
        // Load a binary file from disk into the chip's memory array
        bool load_from_file(const std::string& filename);
        
        // Direct pointer access (useful for debuggers/visualizers)
        u8* get_data_ptr() { return m_data; }

        // --- HARDWARE SIGNALS ---
        // Standard Read/Write interface required by the memory map
        u8 read(u16 addr);
        void write(u16 addr, u8 data);

        // Required by device interface
        void memory_map(address_map& map) override;

    private:
        // --- INTERNAL STORAGE ---
        // WHAT: Fixed array of 32,768 bytes.
        // WHY:  Avoids std::vector dynamic allocation. Matches physical capacity exactly.
        u8 m_data[32768];

        
};