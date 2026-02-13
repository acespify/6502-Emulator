#pragma once
#include "types.h"

// Forward declaration of the Address Map (we will build this later)
class address_map;

// ============================================================================
//  device_memory_interface
// ============================================================================
//  WHAT: A mix-in for devices that master a bus (generate addresses).
//  WHO:  Inherited by the CPU (U1). Used by the CPU to find devices.
// ============================================================================
class device_memory_interface {
public:
    // WHAT: Constructor
    // WHEN: Device creation.
    // WHY:  Sets up the pointer to the address map.
    // HOW:  Initializes to nullptr (safety).
    device_memory_interface() : m_map(nullptr) {}
    
    virtual ~device_memory_interface() = default;

    // ========================================================================
    //  Configuration (Virtual)
    // ========================================================================

    // WHAT: Address Map setup function.
    // WHEN: Called during machine startup.
    // WHY:  The CPU needs to know: "If I read 0x8000, who answers?"
    // HOW:  The specific CPU implementation overrides this to point to 
    //       the specific "Eater 6502" memory map function.
    virtual void memory_map(address_map & /*&map*/) { } // Default is empty map

    // ========================================================================
    //  Memory Accessors
    // ========================================================================
    
    // WHAT: The generic Read Byte function.
    // WHEN: Called by the CPU when executing "LDA $1234".
    // WHY:  Abstracts the complex hardware lookup.
    // HOW:  We will implement the logic later. Ideally, it looks up the 
    //       handler for 'addr' and calls it.
    virtual u8 read_byte(u16 addr);

    // NEW: Debugger Access (No side effects allowed!)
    u8 read_byte_debug(u16 addr);

    // WHAT: The generic Write Byte function.
    // WHEN: Called by the CPU when executing "STA $1234".
    // WHY:  Abstracts the complex hardware lookup.
    virtual void write_byte(u16 addr, u8 data);

protected:
    // WHAT: Pointer to the map.
    // WHEN: Populated during initialization.
    // WHY:  Stores the lookup table of "Address -> Device".
    address_map* m_map = nullptr;
};