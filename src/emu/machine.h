#pragma once
#include "../emu/types.h"

// Define the config stub here so it's visible to drivers
class machine_config {
    public:
    // Global flags can go here later
};

// Generic Machine Interface
class machine {
public:
    virtual ~machine() = default;

    // Lifecycle
    virtual void init() = 0;       // Power On / Allocation
    virtual void reset() = 0;      // Reset Button
    virtual void run(int cycles) = 0; // Execution Slice

    // Accessors for UI
    virtual class m6502_p* get_cpu() = 0;
    virtual class w65c22* get_via() { return nullptr; } // Optional
    virtual class w65c51* get_acia() { return nullptr; }
};