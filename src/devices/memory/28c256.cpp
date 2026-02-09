#include "28c256.h"
#include "../../emu/map.h"
#include <cstdio>  // For fopen, fread
#include <cstring> // For memset
#include <fstream>

// ============================================================================
//  Constructor
// ============================================================================
//  WHAT: Initializes the chip.
//  HOW:  Fills memory with 0xFF (logical '1'). Real EEPROMs read 0xFF when erased.
// ============================================================================
eeprom_28c256::eeprom_28c256() {
    reset_memory();
}

// ============================================================================
//  Reset Memory
// ============================================================================
void eeprom_28c256::reset_memory() {
    // Fill the array with 0xFF.
    // WHY: In an electrically erased state, all bits are 1.
    memset(m_data, 0xFF, sizeof(m_data));
}

// ============================================================================
//  Load From File
// ============================================================================
//  WHAT: Loads a binary ROM image (e.g., "rom.bin") into the array.
//  HOW:  Uses standard C file I/O for simplicity and speed.
// ============================================================================
bool eeprom_28c256::load_from_file(const std::string& filename) {
    /*FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return false;

    // Read up to 32K bytes.
    // If the file is smaller, the rest remains 0xFF.
    // If larger, we truncate.
    fread(m_data, 1, 32768, f);
    fclose(f);
    return true;*/
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("[ROM] Error: Failed to open %s\n", filename); // Debug print
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Safety: Don't overflow the 32KB buffer
    if (size > 32768) size = 32768;

    if (file.read((char*)m_data, size)) {
        printf("[ROM] Loaded %lld bytes from %s\n", size, filename); // Debug print
        return true;
    }
    return false;
}

// ============================================================================
//  Read Byte
// ============================================================================
//  WHAT: Returns the byte at the specified offset.
//  HOW:  Masks address to 15 bits (0-32767) to prevent buffer overflow.
// ============================================================================
u8 eeprom_28c256::read(u16 addr) {
    // The chip only has 15 address lines (A0-A14).
    // Masking with 0x7FFF ensures we wrap around if A15 is high.
    return m_data[addr & 0x7FFF];
}

// ============================================================================
//  Write Byte
// ============================================================================
//  WHAT: Writes a byte to the array.
//  NOTE: Real 28C256 chips support "Software Data Protection" and have a 
//        write cycle time (Wait ~5ms).
//        For this emulator, we treat it like fast RAM for convenience.
// ============================================================================
void eeprom_28c256::write(u16 addr, u8 data) {
    // Mask to 15 bits and write.
    m_data[addr & 0x7FFF] = data;
}

// ============================================================================
//  Memory Map Installation
// ============================================================================
void eeprom_28c256::memory_map(address_map& map) {
    // Install ourselves into the provided map.
    // We bind our 'read' and 'write' functions to the map's delegates.
    map.install(0x0000, 0x7FFF, // Logic range (usually remapped by main system)
        [this](u16 addr) { return this->read(addr); },
        [this](u16 addr, u8 data) { this->write(addr, data); }
    );
}