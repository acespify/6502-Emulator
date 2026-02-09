#pragma once
#include "../../emu/di_memory.h"
#include <cstring> // For memset

// ============================================================================
//  Device: 62256 (32K Static RAM)
// ============================================================================
//  WHO:  Represents the RAM chip (U6 on the Eater Board).
//  WHAT: Stores 32KB of volatile data.
//  WHY:  Provides the Zero Page ($0000) and Stack ($0100).
// ============================================================================
class ram_62256 : public device_memory_interface {
public:
    // Constructor
    ram_62256();
    virtual ~ram_62256() = default;

    // --- HARDWARE SIGNALS ---
    u8 read(u16 addr);
    void write(u16 addr, u8 data);

    // --- UTILITY ---
    // Clears RAM (simulating power cycle)
    void reset_memory();
    
    // Direct pointer access (for Debug UI)
    u8* get_data_ptr() { return m_data; }

    // Required by device interface
    void memory_map(address_map& map) override;

private:
    // --- INTERNAL STORAGE ---
    // WHAT: Fixed array of 32,768 bytes.
    // NOTE: Even if we only map 16K in the 6502 address space, 
    //       the physical chip usually has 32K capacity.
    u8 m_data[32768];
};