#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <string>

// ============================================================================
//  CONFIGURATION
// ============================================================================
const int ROM_SIZE = 32768;
unsigned char rom[ROM_SIZE];
int pc = 0x8000;

// Hardware Pin Mapping (Based on your Schematic)
// All on PORT B ($6000)
const unsigned char RS = 0x01; // PB0
const unsigned char RW = 0x02; // PB1
const unsigned char E  = 0x04; // PB2

// ============================================================================
//  ASSEMBLY HELPERS
// ============================================================================
// These allow us to write "Assembly" inside C++

void emit(unsigned char val) {
    if (pc < 0x8000 + ROM_SIZE) {
        rom[pc - 0x8000] = val;
        pc++;
    }
}

// LDA #immediate
void lda_imm(unsigned char val) {
    emit(0xA9); emit(val);
}

// LDA absolute
void lda_abs(int addr) {
    emit(0xAD); emit(addr & 0xFF); emit(addr >> 8);
}

// STA absolute
void sta_abs(int addr) {
    emit(0x8D); emit(addr & 0xFF); emit(addr >> 8);
}

// LDX #immediate
void ldx_imm(unsigned char val) {
    emit(0xA2); emit(val);
}

// JSR absolute
void jsr(int addr) {
    emit(0x20); emit(addr & 0xFF); emit(addr >> 8);
}

// RTS
void rts() {
    emit(0x60);
}

// ============================================================================
//  SOFTWARE DELAY
// ============================================================================
// Ben's code uses the "Busy Flag" (reading from the LCD). 
// Since 4-bit reads are complex to emulate perfectly, we use a delay loop.
// This is safer and ensures the LCD is ready.
void generate_delay_subroutine() {
    // Label: lcd_wait ($8080)
    pc = 0x8080; 
    
    // LDX #$FF (255 loops)
    ldx_imm(0xFF);
    
    // loop: DEX
    int loop_start = pc;
    emit(0xCA); 
    
    // BNE loop
    emit(0xD0); emit(0xFD); // Jump back 3 bytes
    
    rts();
}

// ============================================================================
//  LCD DRIVER (The Translation Layer)
// ============================================================================
// This replaces Ben's "lcd_instruction" and "print_char".
// Instead of sending 8 bits to Port B, we send two 4-bit nibbles to Port B.

void generate_lcd_send_subroutine() {
    // Label: lcd_send ($8050)
    // Expects: Data in A. 
    // We need to know if it's an Instruction (RS=0) or Character (RS=1).
    // Let's assume we pass the RS flag in the X register for this helper.
    pc = 0x8050;

    // Save A (Data) to Stack so we can use it twice
    emit(0x48); // PHA

    // --- HIGH NIBBLE ---
    // 1. Mask top 4 bits (AND #$F0)
    emit(0x29); emit(0xF0);
    
    // 2. Add RS/RW flags (STX to temp, OR with A... let's keep it simple)
    // We will assume X contains the flags (RS/RW/E).
    // ORA ZeroPage $00 (We'll store flags in ZP $00)
    emit(0x05); emit(0x00);

    // 3. Set E bit (OR #E)
    emit(0x09); emit(E);
    
    // 4. Send to Port B
    sta_abs(0x6000);

    // 5. Clear E bit (AND #~E) -> Triggers the write
    emit(0x29); emit((unsigned char)~E);
    sta_abs(0x6000);

    // --- LOW NIBBLE ---
    // 1. Restore A
    emit(0x68); // PLA
    emit(0x48); // PHA (Save again just in case, though not needed here)

    // 2. Shift Left 4 times (ASL)
    emit(0x0A); emit(0x0A); emit(0x0A); emit(0x0A);

    // 3. Mask top 4 bits
    emit(0x29); emit(0xF0);

    // 4. Add Flags (ORA ZP $00)
    emit(0x05); emit(0x00);

    // 5. Set E
    emit(0x09); emit(E);
    sta_abs(0x6000);

    // 6. Clear E
    emit(0x29); emit((unsigned char)~E);
    sta_abs(0x6000);

    // Restore stack
    emit(0x68); // PLA
    rts();
}

int main() {
    // Fill ROM with NOPs
    memset(rom, 0xEA, ROM_SIZE);

    // Generate Helper Subroutines first so we know their addresses
    generate_delay_subroutine(); // at $8080
    generate_lcd_send_subroutine(); // at $8050

    // ========================================================================
    //  MAIN PROGRAM ($8000)
    // ========================================================================
    pc = 0x8000;

    // --- INIT STACK ---
    ldx_imm(0xFF);
    emit(0x9A); // TXS

    // --- CONFIG VIA ---
    // Ben: Set Port B to Output. Port A to Output.
    // Us: Set Port B to Output (DDRB = $FF).
    lda_imm(0xFF);
    sta_abs(0x6002); // DDRB

    // --- LCD INITIALIZATION (4-BIT MODE) ---
    // Ben's code assumes 8-bit mode. We MUST perform the 4-bit "Wake Up" sequence.
    // Otherwise, the LCD expects 8 bits and gets confused (causing ??T??).

    // 1. Function Set (0x30) - 3 times
    // We manually bit-bang this because our 'lcd_send' helper sends 2 nibbles,
    // but the initialization requires sending ONLY the high nibble 3 times.
    
    // Send 0x30 (High Nibble 3 + E) -> 0x34
    lda_imm(0x34); sta_abs(0x6000); 
    lda_imm(0x30); sta_abs(0x6000); // Pulse E low
    jsr(0x8080); // Wait

    lda_imm(0x34); sta_abs(0x6000); 
    lda_imm(0x30); sta_abs(0x6000);
    jsr(0x8080);

    lda_imm(0x34); sta_abs(0x6000); 
    lda_imm(0x30); sta_abs(0x6000);
    jsr(0x8080);

    // 2. Set 4-Bit Mode (0x20)
    lda_imm(0x24); sta_abs(0x6000); // 0x20 + E
    lda_imm(0x20); sta_abs(0x6000); // Pulse E low
    jsr(0x8080);

    // --- NORMAL CONFIGURATION ---
    // Now we are in 4-bit mode. We can use our 'lcd_send' subroutine.
    // We store the RS flag in ZeroPage $00 for the subroutine to use.

    // 1. Function Set: 4-bit, 2-line, 5x8 (0x28)
    lda_imm(0x00); emit(0x85); emit(0x00); // STA ZP $00 (RS=0)
    lda_imm(0x28); jsr(0x8050); 

    // 2. Display On, Cursor Off (0x0C)
    // Ben used 0x0E (Cursor On). We'll stick to 0x0C for a clean look.
    lda_imm(0x0C); jsr(0x8050);

    // 3. Entry Mode: Inc, No Shift (0x06)
    lda_imm(0x06); jsr(0x8050);

    // 4. Clear Display (0x01)
    lda_imm(0x01); jsr(0x8050);
    // Clear takes a long time, wait extra
    jsr(0x8080); jsr(0x8080);

    // --- PRINT LOOP ---
    // Ben: ldx #0 ... lda message,x
    ldx_imm(0x00);

    int print_loop = pc; // Label 'print'
    
    // LDA message, x ($8200 + X)
    emit(0xBD); emit(0x00); emit(0x82);

    // BEQ loop (Null terminator)
    emit(0xF0); emit(0x0D); // Forward to 'done'

    // JSR print_char
    // We set RS=1 (Character) in ZP $00
    emit(0x48); // PHA (Save Char)
    lda_imm(RS); emit(0x85); emit(0x00); // STA ZP $00
    emit(0x68); // PLA (Restore Char)
    
    jsr(0x8050); // Call lcd_send

    // INX
    emit(0xE8);
    // JMP print
    emit(0x4C); emit(print_loop & 0xFF); emit(print_loop >> 8);

    // done:
    int done_addr = pc;
    emit(0x4C); emit(done_addr & 0xFF); emit(done_addr >> 8); // Infinite loop

    // ========================================================================
    //  DATA SECTION ($8200)
    // ========================================================================
    pc = 0x8200;
    const char* msg = "Hello, World!";
    for (int i = 0; i < strlen(msg); i++) {
        rom[pc - 0x8000] = msg[i];
        pc++;
    }
    rom[pc - 0x8000] = 0; // Null terminator

    // ========================================================================
    //  VECTORS
    // ========================================================================
    rom[0x7FFC] = 0x00; rom[0x7FFD] = 0x80; // RESET -> $8000
    rom[0x7FFE] = 0x00; rom[0x7FFF] = 0x80; // IRQ

    std::ofstream outfile("rom.bin", std::ios::binary);
    outfile.write((char*)rom, ROM_SIZE);
    outfile.close();

    std::cout << "Generated 4-bit adapted rom.bin" << std::endl;
    return 0;
}