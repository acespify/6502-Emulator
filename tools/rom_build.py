import struct

# =============================================================================
# CONSTANTS & CONFIG
# =============================================================================
ROM_SIZE = 32768  # 32 KiB
rom = bytearray([0xEA] * ROM_SIZE)  # Fill with NOPs (0xEA)

# Hardware Addresses
PORTB = 0x6000
PORTA = 0x6001
DDRB  = 0x6002
DDRA  = 0x6003

# Control Flags
E  = 0x80
RW = 0x40
RS = 0x20

# Memory Cursor (Start at $8000)
pc = 0x8000

# =============================================================================
# HELPER FUNCTIONS (The Assembler)
# =============================================================================
def emit(byte):
    global pc
    if 0x8000 <= pc < (0x8000 + ROM_SIZE):
        rom[pc - 0x8000] = byte
        pc += 1

def LDA_IMM(val): emit(0xA9); emit(val)
def LDA_ABS(addr): emit(0xAD); emit(addr & 0xFF); emit(addr >> 8)
def LDA_ABX(addr): emit(0xBD); emit(addr & 0xFF); emit(addr >> 8)
def LDX_IMM(val): emit(0xA2); emit(val)
def LDY_IMM(val): emit(0xA0); emit(val)
def STA_ABS(addr): emit(0x8D); emit(addr & 0xFF); emit(addr >> 8)
def JMP_ABS(addr): emit(0x4C); emit(addr & 0xFF); emit(addr >> 8)
def JSR_ABS(addr): emit(0x20); emit(addr & 0xFF); emit(addr >> 8)
def BEQ(offset): emit(0xF0); emit(offset & 0xFF)
def BNE(offset): emit(0xD0); emit(offset & 0xFF)
def INX(): emit(0xE8)
def DEY(): emit(0x88)
def TXS(): emit(0x9A)
def PHA(): emit(0x48)
def PLA(): emit(0x68)
def RTS(): emit(0x60)
def TXA(): emit(0x8A)
def TAX(): emit(0xAA)
def TYA(): emit(0x98)
def TAY(): emit(0xA8)

# Addresses for Subroutines (Hardcoded to match C++ version layout)
ADDR_LCD_INS   = 0x8100
ADDR_PRT_CHR   = 0x8120
ADDR_WAIT_1MS  = 0x8140
ADDR_WAIT_50MS = 0x8160
ADDR_DATA      = 0x8200

# =============================================================================
# MAIN PROGRAM ($8000)
# =============================================================================
pc = 0x8000

# 1. Hardware Init
LDX_IMM(0xFF); TXS()
LDA_IMM(0xFF); STA_ABS(DDRB)
LDA_IMM(0xE0); STA_ABS(DDRA)

# 2. LCD Wake-Up
LDA_IMM(0x38); JSR_ABS(ADDR_LCD_INS)
LDA_IMM(0x38); JSR_ABS(ADDR_LCD_INS)
LDA_IMM(0x38); JSR_ABS(ADDR_LCD_INS)

# 3. Configuration
LDA_IMM(0x38); JSR_ABS(ADDR_LCD_INS) # 8-bit, 2-line

# Cursor ON, Blink ON (0x0F)
LDA_IMM(0x0F); 
JSR_ABS(ADDR_LCD_INS)

LDA_IMM(0x06); JSR_ABS(ADDR_LCD_INS) # Entry Mode Inc

# 4. Clear Display
LDA_IMM(0x01)
JSR_ABS(ADDR_LCD_INS)

# [FIX] Call the 50ms Delay Subroutine (Nuclear Option)
JSR_ABS(ADDR_WAIT_50MS)

# 5. Print Loop
LDX_IMM(0x00)
print_label = pc

LDA_ABX(ADDR_DATA)   # Load char
BEQ(0x07)            # If 0, jump forward

JSR_ABS(ADDR_PRT_CHR)# Print
INX()
JMP_ABS(print_label)

# Infinite Loop
loop_label = pc
JMP_ABS(loop_label)

# =============================================================================
# SUBROUTINES
# =============================================================================

# --- lcd_instruction ---
pc = ADDR_LCD_INS
JSR_ABS(ADDR_WAIT_1MS)
STA_ABS(PORTB)
LDA_IMM(0); STA_ABS(PORTA) # E=0
LDA_IMM(E); STA_ABS(PORTA) # E=1
LDA_IMM(0); STA_ABS(PORTA) # E=0
RTS()

# --- print_char ---
pc = ADDR_PRT_CHR
JSR_ABS(ADDR_WAIT_1MS)
STA_ABS(PORTB)
LDA_IMM(RS);     STA_ABS(PORTA) # RS=1
LDA_IMM(RS | E); STA_ABS(PORTA) # E=1
LDA_IMM(RS);     STA_ABS(PORTA) # E=0
RTS()

# --- wait_1ms ---
# Simple loop: 255 iterations
pc = ADDR_WAIT_1MS
PHA(); TXA(); PHA()
LDX_IMM(0xFF)
# DEX
wait_loop = pc
emit(0xCA)
# BNE wait_loop (-3 bytes)
emit(0xD0); emit(0xFD)
PLA(); TAX(); PLA()
RTS()

# --- wait_50ms ---
# Nested loop: 255 * 255 iterations
pc = ADDR_WAIT_50MS
PHA(); TXA(); PHA()
TYA(); PHA()

LDY_IMM(0xFF) # Outer
outer_loop = pc

LDX_IMM(0xFF) # Inner
inner_loop = pc

emit(0xCA) # DEX
# BNE inner_loop (-3 bytes)
emit(0xD0); emit(0xFD)

DEY()
# BNE outer_loop (-8 bytes: LDX(2)+DEX(1)+BNE(2)+DEY(1)+BNE(2) = 8)
emit(0xD0); emit(0xF8)

PLA(); TAY(); PLA(); TAX(); PLA()
RTS()

# =============================================================================
# DATA
# =============================================================================
pc = ADDR_DATA
msg = b"Hello, world!\x00"
for b in msg:
    emit(b)

# =============================================================================
# VECTORS
# =============================================================================
rom[0x7FFC - 0x8000] = 0x00
rom[0x7FFD - 0x8000] = 0x80
rom[0x7FFE - 0x8000] = 0x00
rom[0x7FFF - 0x8000] = 0x80

# =============================================================================
# WRITE FILE
# =============================================================================
with open("rom.bin", "wb") as f:
    f.write(rom)

print("rom.bin generated (Nuclear Option: Safe Delays + Blinking Cursor)")