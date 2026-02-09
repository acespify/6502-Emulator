//======================================================================================
//  This file is the base class for the eater6502 emulator being developed as a 
//  tutorial for a youtube video on "How to create a MAME Style" emulator.
//  This will be part of the beginning video series. Each new step will be added to
//  each file and labeled accordingly.
//======================================================================================
#pragma once
#include <string>
#include <iostream>
#include "types.h"

// Forward declaration: The "Machine" holds the list of all devices (the motherboard)
class machine_config;

class device_t {
    public:
        // --- 1. Construction ---
        // Every device needs:
        // - machine: A reference to the main board
        // - tag: A unique name (e.g., "u1_cpu")
        // - owner: The parent device (usually the machine root)
        // - clock: The operating frequency (e.g., 1_000_000 for 1MHz)
        device_t(machine_config &mconfig, const std::string& tag, device_t *owner, u32 clock);
            
        // Destructor
        virtual ~device_t() = default;

        // --- 2. Interface (Polymorphism) ---
        // These are pure virtual or virtual methods that specific chips MUST or CAN override.

        // Hardware Power-On (Required)
        // Used to allocate memory, find other devices, and register save states.
        virtual void device_start() = 0;

        // Hardware Reset (Optional)
        // Used to zero out registers and set PC to reset vector.
        // Default behavior: do nothing.
        virtual void device_reset() { }

        // Hardware Power-Off (Optional)
        virtual void device_stop() { }

        // ===== Getters / Indentification =====

        const std::string& tag() const { return m_tag; }
        u32 clock() const { return m_clock; }

        // Returns the full path (e.g., ":maincpu") - MAME uses colon separators
        std::string qname() const {
            if(m_owner) return m_owner->qname() + ":" + m_tag;
            return ":" + m_tag;
        }

        // Accessor for the main machine object
        machine_config& machine() const { return m_machine; }

    protected:
        machine_config&     m_machine;      // Reference to the system coordinator
        std::string         m_tag;          // Local name
        device_t*           m_owner;        // Parent device
        u32                 m_clock;        // The operating Speed (hz)
};