#pragma once
#include "../../types.h"

// ============================================================================
//  Device: 74HC00 (Quad 2-Input NAND Gate)
// ============================================================================
//  WHO:  The "Glue Logic" chip.
//  WHAT: Contains 4 independent NAND gates.
//  WHY:  Used to decode addresses (e.g., A15 NAND A15 -> NOT A15).
// ============================================================================
class logic_74hc00 {
public:
    // ========================================================================
    //  Gate Logic
    // ========================================================================
    //  WHAT: Performs the NAND operation.
    //  HOW:  Returns 1 if (A & B) is 0. Returns 0 if (A & B) is 1.
    //        Using u8 (0 or 1) for signal logic.
    static u8 nand(u8 input_a, u8 input_b) {
        // C++ boolean logic: (a && b) returns true/false.
        // We invert it (!) and cast to u8.
        return !(input_a && input_b);
    }
    
    // ========================================================================
    //  Helper: Inverter Config
    // ========================================================================
    //  WHAT: Frequently used as an inverter by tying inputs together.
    //  HOW:  NAND(A, A) == NOT(A).
    static u8 not_gate(u8 input_a) {
        return nand(input_a, input_a);
    }
};