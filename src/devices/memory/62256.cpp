#include "62256.h"
#include "../../emu/map.h"

// ============================================================================
//  Constructor
// ============================================================================
ram_62256::ram_62256() {
    reset_memory();
}

// ============================================================================
//  Reset Memory
// ============================================================================
//  WHAT: Wipes the RAM.
//  WHY:  SRAM is volatile; data is lost on power loss.
// ============================================================================
void ram_62256::reset_memory() {
    // Fill with 0 (Clean state)
    // Optional: Fill with 0xCC or random data to detect uninitialized variable bugs.
    memset(m_data, 0x00, sizeof(m_data));
}

// ============================================================================
//  Read Byte
// ============================================================================
u8 ram_62256::read(u16 addr) {
    // Mask to 32K range (A0-A14)
    return m_data[addr & 0x7FFF];
}

// ============================================================================
//  Write Byte
// ============================================================================
void ram_62256::write(u16 addr, u8 data) {
    // Write to array
    m_data[addr & 0x7FFF] = data;
}

// ============================================================================
//  Memory Map Installation
// ============================================================================
void ram_62256::memory_map(address_map& map) {
    // Install read/write handlers
    map.install(0x0000, 0x7FFF, 
        [this](u16 addr) { return this->read(addr); },
        [this](u16 addr, u8 data) { this->write(addr, data); }
    );
}