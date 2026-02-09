#pragma once

#include <functional>
#include <iostream>
#include "types.h"

// ============================================================================
//  Delegate Definitions
// ============================================================================
//  WHAT: Aliases for the function types used to read and write memory.
//  WHEN: Used when defining the map entries and passing data between chips.
//  WHY:  Simplifies the code. Instead of typing "std::function<u8(u16)>" everywhere,
//        we just type "read8_delegate".
//  HOW:  The 'using' keyword creates a readable alias.
// ============================================================================

// A function that accepts an Address (u16) and returns a Byte (u8)
using read8_delegate  = std::function<u8(u16)>;

// A function that accepts an Address (u16) and a Byte (u8) and returns void
using write8_delegate = std::function<void(u16, u8)>;

// ============================================================================
//  struct map_entry
// ============================================================================
//  WHAT: A single row in our address lookup table.
//  WHO:  Created by the user (you) when you call 'install()' in the machine config.
// ============================================================================
struct map_entry {
    // WHAT: Start Address (Inclusive).
    // WHEN: Checked every time the CPU performs a read or write cycle.
    // WHY:  Defines the lower bound of this device's memory range.
    u16 m_start;

    // WHAT: End Address (Inclusive).
    // WHEN: Checked every time the CPU performs a read or write cycle.
    // WHY:  Defines the upper bound of this device's memory range.
    u16 m_end;

    // WHAT: The "Read" Callback.
    // WHEN: Executed if the CPU is in 'Read Mode' (e.g., LDA instruction) 
    //       and the address is within range.
    // WHY:  Delegates the actual work to the specific hardware device (RAM, ROM, etc).
    read8_delegate m_read;

    // WHAT: The "Debug" read (safe, no side effects)
    read8_delegate m_read_debug;

    // WHAT: The "Write" Callback.
    // WHEN: Executed if the CPU is in 'Write Mode' (e.g., STA instruction)
    //       and the address is within range.
    // WHY:  Delegates the actual work to the specific hardware device.
    write8_delegate m_write;
};

// ============================================================================
//  class address_map
// ============================================================================
//  WHAT: The memory controller unit (MMU) logic.
//  WHO:  Owned by the CPU. The CPU asks this class to fetch data.
// ============================================================================
class address_map {
public:
    // WHAT: Constant for maximum devices.
    // WHY:  Replaces std::vector. We allocate a fixed block of memory.
    //       64 is plenty for an 8-bit computer (RAM, ROM, VIA, ACIA = 4 entries).
    static constexpr int MAX_ENTRIES = 64;

    // WHAT: Constructor.
    // WHEN: When the CPU is created.
    // WHY:  Initializes the counter to zero so we know the list is empty.
    // HOW:  Simple assignment.
    address_map() : m_count(0) {}

    // ========================================================================
    //  Installation (Building the Board)
    // ========================================================================

    // WHAT: The Install Function.
    // WHEN: Called during 'machine_start' to wire up devices.
    // WHY:  Adds a new device to our list of searchable memory ranges.
    // HOW:  1. Checks if we have room in the array.
    //       2. Populates the next available 'map_entry' slot.
    //       3. Increments the counter.
    void install(u16 start, u16 end, read8_delegate r, write8_delegate w) {
        // Safety Check: Do we have space?
        if (m_count >= MAX_ENTRIES) {
            std::cerr << "Fatal Error: Address Map overflow! Too many devices." << std::endl;
            return;
        }

        // Access the next empty slot using our counter
        map_entry& entry = m_entries[m_count];
        
        // Fill the data
        entry.m_start = start;
        entry.m_end   = end;
        entry.m_read  = r;
        entry.m_write = w;

        // DEFAULT: The debug reader is the same as the real reader.
        // This is correct for 99% of cases (RAM, ROM).
        entry.m_read_debug = r;

        // Increment the counter so the next install goes to the next slot
        m_count++;
    }

    // Helper to override the debug reader for a specific range
    // You call this AFTER calling install() if you need a special safe handler.
    void install_debug_handler(u16 start, u16 end, read8_delegate r_debug) {
        // Find the existing entry that matches this range
        for (int i = 0; i < m_count; i++) {
            if (m_entries[i].m_start == start && m_entries[i].m_end == end) {
                // Found it! Override the debug pointer.
                m_entries[i].m_read_debug = r_debug;
                return;
            }
        }
        std::cerr << "Warning: Could not find map entry to override debug handler for " 
                  << std::hex << start << std::endl;
    }

    // ========================================================================
    //  Access (Running the Emulation)
    // ========================================================================

    // WHAT: The Read Lookup.
    // WHEN: Called by the CPU (every instruction cycle).
    // WHY:  Resolves a 16-bit address to a specific byte of data.
    // HOW:  Iterates through the fixed array up to 'm_count'.
    u8 read(u16 addr) {
        // Loop through all installed devices
        for (int i = 0; i < m_count; i++) {
            // Check if address falls inside this entry's range
            if (addr >= m_entries[i].m_start && addr <= m_entries[i].m_end) {
                
                // If a read function was provided, call it!
                if (m_entries[i].m_read) {
                    return m_entries[i].m_read(addr);
                }
            }
        }
        // Fallback: If no device responds (Open Bus), return 0.
        // In real hardware, this might be floating voltage, but 0 is safe.
        return 0x00; 
    }

    u8 read_debug(u16 addr) {
        for (int i = 0; i < m_count; i++) {
            if (addr >= m_entries[i].m_start && addr <= m_entries[i].m_end) {
                // Use the DEBUG delegate here!
                if (m_entries[i].m_read_debug) {
                    return m_entries[i].m_read_debug(addr);
                }
            }
        }
        return 0x00; 
    }

    // WHAT: The Write Lookup.
    // WHEN: Called by the CPU (e.g., STA $6000).
    // WHY:  Delivers data to the correct chip.
    // HOW:  Iterates through the fixed array.
    void write(u16 addr, u8 data) {
        for (int i = 0; i < m_count; i++) {
            if (addr >= m_entries[i].m_start && addr <= m_entries[i].m_end) {
                
                // If a write function was provided, call it!
                if (m_entries[i].m_write) {
                    m_entries[i].m_write(addr, data);
                    return; // We found the target, no need to keep searching.
                }
            }
        }
    }

private:
    // ========================================================================
    //  Internal Storage
    // ========================================================================
    
    // WHAT: The Fixed Array.
    // WHEN: Allocated when the address_map is created.
    // WHY:  Stores the rules. No dynamic memory allocation (no 'new' or 'malloc').
    // HOW:  A standard C array of structs.
    map_entry m_entries[MAX_ENTRIES];

    // WHAT: The Item Counter.
    // WHEN: Incremented on install, used during read/write loops.
    // WHY:  Since the array is fixed size (64), we need to know how many 
    //       slots are actually being used (e.g., 4).
    int m_count;
};