#pragma once

#include "types.h"

// Forward declaration of device_t so we can reference the parent if needed.
class device_t;

// ============================================================================
//  device_execute_interface
// ============================================================================
//  WHAT: A mix-in class that allows a device to be scheduled and execute instructions.
//  WHO:  Inherited by CPUs (U1 6502). The Machine Scheduler calls these methods.
// ============================================================================
class device_execute_interface {
    public:
        // WHAT: Constructor.
        // WHEN: Called when the specific chip (e.g., m6502) is created.
        // WHY:  Initializes the cycle counters to zero.
        // HOW:  Standard C++ initialization list.
        device_execute_interface() : m_icount(0) { }

        // WHAT: Virtual Destructor.
        // WHEN: When the emulator shuts down.
        // WHY:  Ensures proper cleanup of derived classes.
        virtual ~device_execute_interface() = default;

        // ========================================================================
        //  The Contract (Pure Virtual Methods)
        //  The specific chip MUST write the code for these.
        // ========================================================================

        // WHAT: The main execution loop.
        // WHEN: Called by the Scheduler when it's this CPU's turn to run.
        // WHY:  This is where the CPU fetches opcodes and does work.
        // HOW:  The CPU will loop until 'm_icount' drops to zero.
        virtual void execute_run() = 0;

        // ========================================================================
        //  Public API (Methods the rest of the emulator calls)
        // ========================================================================

        // WHAT: Cycle counter accessor.
        // WHEN: Needed by debuggers or other devices to see how much time passed.
        // WHY:  Keeps the internal counter protected while allowing read access.
        // HOW:  Returns the standard integer value.
        s32 icount() const { return m_icount; }

        // WHAT: Cycle Adjuster.
        // WHEN: Called by the CPU implementation after an instruction finishes.
        // WHY:  The Scheduler gives us 1000 cycles. If an instruction takes 2, 
        //       we subtract 2. When we hit 0, we yield back to the Scheduler.
        // HOW:  Simple subtraction.
        void icount_consume(s32 cycles) { m_icount -= cycles; }

        // WHAT: Cycle Setter (The "Gas Tank").
        // WHEN: Called by the Scheduler BEFORE calling execute_run().
        // WHY:  "Refuels" the CPU with a budget of cycles to burn for this frame.
        // HOW:  Direct assignment.
        void icount_set(s32 cycles) { m_icount = cycles; }

    protected:
        // ========================================================================
        //  Internal State
        // ========================================================================

        // WHAT: Interrupt Count / Instruction Count.
        // WHEN: Updated constantly during execute_run().
        // WHY:  This is the "budget" of time the CPU has left to run.
        // HOW:  Signed integer allows it to go slightly negative (overshoot), 
        //       which is important for accurate timing accumulation.
        s32 m_icount;
};