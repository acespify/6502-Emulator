//======================================================================
// The first step in the coding process: The way MAME developers and 
// low level coders in general, create a easier way to define a strict,
// short-hand types
//
// This file will guarantee that when u8 is used, we mean exactly 8bits
// whether this is done on a Window or Linux.
//======================================================================
#pragma once

#include <cstdint>

// MAME relies on a heavely strict interger sizes
// Standard MAME-style short names for easier typing
using u8 = uint8_t;     // 8-bit unsigned (0 to 255)
using u16 = uint16_t;   // 16-bit unsigned (0 to 65,535)
using u32 = uint32_t;   // 32-bit unsigned (0 to 4,294,967,295)
using u64 = uint64_t;   // 64-bit unsigned (Huge)

using s8 = int8_t;      // 8-bit signed     (-128 to 127)
using s16 = int16_t;    // 16-bit signed    (-32,768 to 32,767)
using s32 = int32_t;    // 32-bit signed    
using s64 = int64_t;    // 64-bit signed

// Helpful constants for the 6502 coding
constexpr u16 K = 1024;
constexpr u32 M = K * K;