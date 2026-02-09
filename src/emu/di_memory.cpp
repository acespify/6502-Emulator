#include "di_memory.h"
#include "map.h" // Include our new Map definition

// ============================================================================
//  di_memory::read_byte
// ============================================================================
//  WHAT: Public API for reading memory.
//  WHEN: Called by the CPU execution loop (fetch opcode, fetch operand).
//  WHY:  The CPU doesn't know about 'm_entries' or loops. It just wants a byte.
//  HOW:  Delegates the search task to the address_map object.
// ============================================================================
u8 device_memory_interface::read_byte(u16 addr) {
    // Safety Check: Does this CPU have a map assigned?
    if (m_map) {
        return m_map->read(addr);
    }
    
    // If no map, return 0.
    return 0x00;
}

// ============================================================================
//  di_memory::write_byte
// ============================================================================
//  WHAT: Public API for writing memory.
//  WHEN: Called by the CPU execution loop (STA, STX, STY).
//  WHY:  Passes the data from the CPU register to the memory bus.
//  HOW:  Delegates the search task to the address_map object.
// ============================================================================
void device_memory_interface::write_byte(u16 addr, u8 data) {
    if (m_map) {
        m_map->write(addr, data);
    }
}

// ============================================================================
//  Debug Access (No Side Effects)
// ============================================================================
u8 device_memory_interface::read_byte_debug(u16 addr) {
        if (m_map) {
            // Call the new specific map function
            return m_map->read_debug(addr);
        }
        return 0xFF; // Default open bus value
    }