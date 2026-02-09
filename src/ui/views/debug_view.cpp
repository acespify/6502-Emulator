#include "debug_view.h"
#include "../../driver/mainboard.h"       // Required for mb_driver->get_cpu()
#include "../../devices/video/nhd_0216k1z.h" // Required for m_lcd->get_display_lines()
#include "../../../vendor/imgui/imgui.h"
#include <cstdio>
// We need the CPU definition to access registers (A, X, Y, PC)
// Ensure this path matches where you put your CPU file
#include "devices/cpu/m6502.h" 

// ============================================================================
// Constructor
// ============================================================================
//  WHAT: Initializes the Debug View controller.
//  WHEN: Created by the main application (Renderer) at startup.
//  WHY:  Stores the pointers to the hardware devices so we can read their state later.
// ============================================================================
DebugView::DebugView(mb_driver* driver)
    : m_driver(driver) 
{
    // Extract pointers for easy access
    m_cpu = driver->get_cpu();
    m_via = driver->get_via();
    m_acia = driver->get_acia();
    m_lcd = driver->get_lcd();
}

// ============================================================================
// Main Draw Loop
// ============================================================================
//  WHAT: The Master UI Function.
//  WHEN: Called once per frame by the Renderer.
//  WHY:  Orchestrates which windows are drawn based on user selection.
// ============================================================================
void DebugView::draw(bool& is_paused, bool& step_request) {
    
    // 1. Draw Top Menu
    draw_menu_bar(is_paused, step_request);

    // 2. Draw Windows (if enabled)
    if (m_show_cpu)     draw_cpu_window(is_paused, step_request);
    if (m_show_stack)   draw_stack_smart();
    if (m_show_via)     draw_via_window();
    if (m_show_acia)    draw_acia_window();
    if (m_show_ram)     draw_memory_window();
    if (m_show_lcd)     draw_lcd_window();
    if (m_show_rom)     draw_rom_window();
}

// ============================================================================
// Menu Bar
// ============================================================================
void DebugView::draw_menu_bar(bool& is_paused, bool& step_request) {
    if (ImGui::BeginMainMenuBar()) {
        
        // Execution Controls
        if (ImGui::BeginMenu("System")) {
            if (is_paused) {
                if (ImGui::MenuItem("Resume")) is_paused = false;
                if (ImGui::MenuItem("Step Instruction")) step_request = true;
            } else {
                if (ImGui::MenuItem("Pause")) is_paused = true;
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Reset CPU")) {
                if (m_cpu) m_cpu->device_reset();
            }
            ImGui::EndMenu();
        }

        // Window Toggles
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("CPU Registers", nullptr, &m_show_cpu);
            ImGui::MenuItem("Stack Viewer",  nullptr, &m_show_stack);
            ImGui::MenuItem("VIA (U5)",      nullptr, &m_show_via);
            ImGui::MenuItem("ACIA (U7)",     nullptr, &m_show_acia);
            ImGui::MenuItem("Memory Dump",   nullptr, &m_show_ram);
            ImGui::MenuItem("Rom",           nullptr, &m_show_rom);
            ImGui::MenuItem("LCD Display",   nullptr, &m_show_lcd);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

// ============================================================================
// ROM Loader Window (NEW)
// ============================================================================
//  WHAT: A simple window to pick a file and load it.
//  WHY:  Allows switching between "rom.bin", "wozmon.bin", etc. without recompiling.
// ============================================================================
void DebugView::draw_rom_window() {
    ImGui::Begin("ROM Loader");

    ImGui::Text("Load Firmware Image");
    
    // Input Box for Filename
    ImGui::InputText("Filename", m_rom_path, 256);
    
    // Load Button
    if (ImGui::Button("Load & Reset")) {
        // 1. Call Driver to Load
        if (m_driver->load_rom(m_rom_path)) {
            // 2. Reset CPU to load new Vector
            m_driver->reset();
            snprintf(m_status_msg, 128, "Success: Loaded %s", m_rom_path);
        } else {
            snprintf(m_status_msg, 128, "Error: File not found!");
        }
    }

    // Status Message (Yellow)
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", m_status_msg);

    ImGui::End();
}

// ============================================================================
// CPU Window
// ============================================================================
//  WHAT: Displays the internal state of the W65C02S.
//  WHEN: Every frame if 'm_show_cpu' is true.
//  WHY:  Vital for debugging. Shows us exactly what the processor is thinking.
// ============================================================================
void DebugView::draw_cpu_window(bool& is_paused, bool& step_request) {
    ImGui::Begin("W65C02 CPU (U1)");

    if (m_cpu) {
        // We assume getters exist in m6502.h 
        // If not, you will need to add them (e.g., u8 get_a() { return A; })
        
        // Row 1: Main Registers
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "PC: %04X", m_cpu->get_pc()); // Direct access if public, else get_pc()
        ImGui::SameLine();
        ImGui::Text("A: %02X", m_cpu->get_a());
        ImGui::SameLine();
        ImGui::Text("X: %02X", m_cpu->get_x());
        ImGui::SameLine();
        ImGui::Text("Y: %02X", m_cpu->get_y());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 1, 0, 1),"Stack: %04X", m_cpu->get_sp());

        ImGui::Separator();

        // Row 2: Flags Breakdown [N V - B D I Z C]
        u8 p = m_cpu->get_flags(); // or get_p()
        
        ImGui::Text("Flags:"); ImGui::SameLine();
        
        auto DrawFlag = [&](const char* name, u8 mask) {
            if (p & mask) ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", name);
            else          ImGui::TextDisabled("%s", name);
            ImGui::SameLine();
        };

        DrawFlag("N", 0x80);
        DrawFlag("V", 0x40);
        DrawFlag("-", 0x20); //ImGui::TextDisabled("-"); ImGui::SameLine();
        DrawFlag("B", 0x10);
        DrawFlag("D", 0x08);
        DrawFlag("I", 0x04);
        DrawFlag("Z", 0x02);
        DrawFlag("C", 0x01);
        ImGui::NewLine();

        ImGui::Separator();

        // Control Buttons inside the window
        if (is_paused) {
             if (ImGui::Button("Step")) step_request = true;
             ImGui::SameLine();
             if (ImGui::Button("Run")) is_paused = false;
        } else {
             if (ImGui::Button("Pause")) is_paused = true;
        }

    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "CPU Not Connected");
    }

    ImGui::End();
}

// ============================================================================
//  Smart Stack Window
// ============================================================================
void DebugView::draw_stack_smart() {
    ImGui::Begin("Stack Visualizer (Page 1)");

    if (m_cpu) {
        u8 sp = m_cpu->get_sp(); // e.g., 0xFD
        
        // 1. Calculate how many items are on the stack.
        //    Stack starts at FF. If SP is FD, we have used FF and FE (2 items).
        int bytes_pushed = 0xFF - sp;

        ImGui::Text("Stack Pointer (S): %02X", sp);
        ImGui::Text("Depth: %d bytes", bytes_pushed);
        ImGui::Separator();

        if (bytes_pushed == 0) {
            ImGui::TextDisabled("Stack Empty (SP = FF)");
        }
        else {
            // Table for pretty alignment
            if (ImGui::BeginTable("stack_table", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
                
                ImGui::TableSetupColumn("Addr");
                ImGui::TableSetupColumn("Val");
                ImGui::TableSetupColumn("Interpretation");
                ImGui::TableHeadersRow();

                // 2. Iterate from BOTTOM (01FF) to TOP (SP + 1)
                //    We go backwards to show the "First In" at the top of our list?
                //    Actually, stacks are usually visualized "Top Down" (Latest on top).
                //    Let's loop from SP+1 (Latest) up to 0xFF (Oldest).
                
                for (int i = sp + 1; i <= 0xFF; i++) {
                    u16 addr = 0x0100 + i;
                    u8 val = m_cpu->debug_peek(addr);

                    ImGui::TableNextRow();
                    
                    // Col 1: Address
                    ImGui::TableNextColumn();
                    ImGui::Text("%04X", addr);

                    // Col 2: Value
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "%02X", val);

                    // Col 3: Hints
                    ImGui::TableNextColumn();
                    
                    // HEURISTIC: Try to guess what this is.
                    // If it's the JSR test, we pushed Return Addr (Low), then Return Addr (High)?
                    // No, 6502 pushes High then Low.
                    // So 01FF = High, 01FE = Low.
                    
                    if (i == 0xFF) ImGui::TextDisabled("Base (01FF)");
                    else if (i == sp + 1) ImGui::TextColored(ImVec4(0,1,0,1), "Top of Stack");
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
}

// ============================================================================
// Placeholders (To be filled as you build classes)
// ============================================================================

void DebugView::draw_via_window() {
    ImGui::Begin("VIA (U5) - I/O Controller");

    if (m_via) {
        // Helper to draw 8 bits
        auto DrawBinary = [](const char* label, u8 val) {
            ImGui::Text("%s: %02X  [", label, val);
            for (int i = 7; i >= 0; i--) {
                ImGui::SameLine();
                if ((val >> i) & 1) ImGui::TextColored(ImVec4(0,1,0,1), "1");
                else                ImGui::TextDisabled("0");
            }
            ImGui::SameLine(); ImGui::Text("]");
        };

        // Reads without side effects using peek()
        // Register 0=ORB, 1=ORA, 2=DDRB, 3=DDRA
        u8 orb  = m_via->peek(0); 
        u8 ora  = m_via->peek(1); 
        u8 ddrb = m_via->peek(2); 
        u8 ddra = m_via->peek(3);
        
        // PORT B (Connected to LCD)
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "PORT B (LCD Data)");
        DrawBinary("DDRB", ddrb); // 1=Output, 0=Input
        DrawBinary("ORB ", orb);  // The actual data being sent

        // PORT A (Unused in your schematic, but good to see)
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "PORT A (Unused)");
        DrawBinary("DDRA", ddra);
        DrawBinary("ORA ", ora);

        ImGui::Separator();
        
        // Interrupt Flags
        u8 ifr = m_via->peek(13); // IFR
        u8 ier = m_via->peek(14); // IER
        ImGui::Text("Interrupts (IFR): %02X", ifr);
        ImGui::Text("Enabled    (IER): %02X", ier);
        
        if (ifr & 0x80) ImGui::TextColored(ImVec4(1,0,0,1), ">>> IRQ ACTIVE <<<");

    } else {
        ImGui::TextDisabled("VIA not connected");
    }
    ImGui::End();
}

void DebugView::draw_acia_window() {
    ImGui::Begin("ACIA (U7) - Serial");
    
    if (m_acia) {
        // W65C51 Registers: 0=Data, 1=Status, 2=Command, 3=Control
        u8 status = m_acia->read(1); // Read status (Note: might clear IRQs in some emus)
        u8 cmd    = m_acia->read(2);
        u8 ctrl   = m_acia->read(3);

        ImGui::Text("Status:  %02X", status);
        ImGui::Text("Command: %02X", cmd);
        ImGui::Text("Control: %02X", ctrl);
        
        ImGui::Separator();
        
        // Decode Status Flags
        if (status & 0x80) ImGui::TextColored(ImVec4(1,0,0,1), "IRQ Active");
        if (status & 0x10) ImGui::Text("Tx Empty");
        if (status & 0x08) ImGui::TextColored(ImVec4(0,1,0,1), "Rx Full (Data Available)");
        
        ImGui::Separator();
        ImGui::TextDisabled("(Serial Terminal not implemented yet)");
    }
    ImGui::End();
}

// ============================================================================
//  Memory Hex Dump Window
// ============================================================================
void DebugView::draw_memory_window() {
    ImGui::Begin("Memory Dump");

    if (m_cpu) {
        // Top Bar: Input box to jump to an address
        static char addr_buf[5] = "0000";
        static int jump_addr = 0x0000;
        static bool trigger_scroll = false; 
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputText("Jump To", addr_buf, 5,
             ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) 
        {
            int parsed_val = 0;
            if (sscanf(addr_buf, "%x", &parsed_val) == 1) {
                jump_addr = parsed_val;
                // Clamp to 16-bit range
            if (jump_addr < 0) jump_addr = 0;
            if (jump_addr > 0xFFFF) jump_addr = 0xFFFF;
            trigger_scroll = true;
            }   
        }

        ImGui::Separator();

        // Hex Dump Table
        // We use a clipper to handle 65536 rows efficiently (only draws what is visible)
        ImGui::BeginChild("HexScrolling");
        
        // We calculate where the row is and force ImGui to scroll there.
        if (trigger_scroll) {
            int row = jump_addr / 16;
            // Get height of one line of text
            float line_height = ImGui::GetTextLineHeightWithSpacing(); 
            ImGui::SetScrollY(row * line_height);
            trigger_scroll = false; // Reset flag
        }

        ImGuiListClipper clipper;
        clipper.Begin(0x1000); // 4096 rows of 16 bytes = 64KB total

        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                u16 base_addr = row * 16;
                
                // 1. Draw Address Column (e.g., "00F0: ")
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "%04X: ", base_addr);
                
                // 2. Draw 16 Bytes
                for (int col = 0; col < 16; col++) {
                    ImGui::SameLine();
                    u16 addr = base_addr + col;
                    
                    // === THE CRITICAL PART ===
                    // We use debug_peek() here. 
                    // This ensures reading $9000 won't trigger your "BOOM" trap.
                    u8 val = m_cpu->debug_peek(addr);

                    // Color code zero vs non-zero for readability
                    if (val == 0) 
                        ImGui::TextDisabled("00");
                    else 
                        ImGui::Text("%02X", val);
                }

                // 3. Draw ASCII representation (Optional but nice)
                ImGui::SameLine();
                ImGui::Text(" | ");
                for (int col = 0; col < 16; col++) {
                    ImGui::SameLine();
                    u16 addr = base_addr + col;
                    u8 val = m_cpu->debug_peek(addr);
                    
                    // Only draw printable chars
                    if (val >= 32 && val < 127) 
                        ImGui::Text("%c", val);
                    else 
                        ImGui::TextDisabled(".");
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

// ============================================================================
//  2. LCD Window (Visualizing U3)
// ============================================================================
void DebugView::draw_lcd_window() {
    ImGui::Begin("LCD Display (U3)");

    // 1. Set Retro Colors (Yellow-Green Background, Dark Grey Text)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.60f, 0.80f, 0.20f, 1.0f)); // #99CC33
    ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(0.10f, 0.15f, 0.05f, 1.0f)); // Dark Green/Grey

    // 2. Create the LCD Screen Box
    ImGui::BeginChild("Screen", ImVec2(0, 80), true); // 80px height
    
    // Set Font Scale (Make it look blocky/retro if possible)
    ImGui::SetWindowFontScale(2.0f);

    const auto& lines = m_lcd->get_display_lines();
    u8 cursor_addr = m_lcd->get_cursor_addr();
    bool cursor_vis = m_lcd->is_cursor_on();

    // Loop through 2 lines
    for (int row = 0; row < 2; row++) {
        // Construct the line string
        std::string line_text = lines[row];
        
        // Print the line
        ImGui::Text("%s", line_text.c_str());

        // --- DRAW CURSOR MANUALLY ---
        if (cursor_vis) {
            int cursor_row = (cursor_addr >= 0x40) ? 1 : 0;
            int cursor_col = (cursor_addr & 0x0F);

            if (row == cursor_row && cursor_col < 16) {
                // Get position of the start of the text line
                ImVec2 line_pos = ImGui::GetItemRectMin();
                
                // Approximate character size (Monospace font is best for this)
                float char_w = ImGui::GetFontSize() * 0.5f; 
                float char_h = ImGui::GetFontSize();

                // Calculate exact screen coordinates for the cursor
                ImVec2 cursor_screen_pos = ImVec2(
                    line_pos.x + (cursor_col * char_w), 
                    line_pos.y
                );

                // Draw Blinking Block
                // Blink rate: Every 0.5 seconds
                bool blink_state = (ImGui::GetTime() - (int)ImGui::GetTime()) > 0.5f;
                
                if (m_lcd->is_blink_on() && blink_state) {
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        cursor_screen_pos,
                        ImVec2(cursor_screen_pos.x + char_w, cursor_screen_pos.y + char_h),
                        IM_COL32(20, 40, 10, 255) // Dark Green Block
                    );
                }
                // If blink is off but cursor is on, draw underscore (optional)
                else if (!m_lcd->is_blink_on()) {
                     ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(cursor_screen_pos.x, cursor_screen_pos.y + char_h - 2),
                        ImVec2(cursor_screen_pos.x + char_w, cursor_screen_pos.y + char_h),
                        IM_COL32(20, 40, 10, 255)
                    );
                }
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2); // Restore Colors

    ImGui::TextDisabled("Controller: ST7066U (8-Bit Mode)");
    ImGui::End();
}

// ============================================================================
// Helper: Draw Byte Header
// ============================================================================
//  WHAT: Draws "00 01 02 ... 0F" row.
//  WHY:  Used at the top of memory dumps to align columns.
// ============================================================================
void DebugView::draw_byte_header(int columns, const char* padding) {
    ImGui::Text("%s", padding);
    for (int i = 0; i < columns; i++) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%02X", i);
    }
    ImGui::Separator();
}