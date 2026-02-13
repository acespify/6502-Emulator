#include "28c256.h"
#include "../../emu/map.h"
#include <iostream>  // For fopen, fread
#include <cstring> // For memset
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

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
     // 1. Try opening exactly as passed
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        // 2. If failed, check if we are in 'bin/' or 'build/' and file is in root
        if (fs::exists("../" + filename)) {
            std::cout << "[ROM] Found file in parent directory. Trying ../" << filename << std::endl;
            file.open("../" + filename, std::ios::binary | std::ios::ate);
        }
    }

    if (!file.is_open()) {
        // 3. One last check: maybe it's in a 'roms/' folder?
        if (fs::exists("roms/" + filename)) {
            file.open("roms/" + filename, std::ios::binary | std::ios::ate);
        }
    }

    if (!file.is_open()) {
        std::cerr << "[ROM] Error: Could not open " << filename << " (Checked CWD, ../, and roms/)" << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > 32768) {
        std::cerr << "[ROM] Warning: File size (" << size << ") larger than 32KB. Truncating." << std::endl;
        size = 32768;
    }

    if (file.read((char*)m_data, size)) {
        std::cout << "[ROM] Successfully loaded " << size << " bytes." << std::endl;
        return true;
    }

    std::cerr << "[ROM] Error reading file data." << std::endl;
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