#include "debug_view.h"
#include "../../driver/mainboard.h"       // Required for mb_driver->get_cpu()
#include "../../devices/video/nhd_0216k1z.h" // Required for m_lcd->get_display_lines()
#include "../../../vendor/imgui/imgui.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>
#include <windows.h>
#include <vector>

// We need the CPU definition to access registers (A, X, Y, PC)
// Ensure this path matches where you put your CPU file
#include "devices/cpu/m6502.h" 

std::vector<LogEntry> DebugView::m_logs;

void DebugView::add_log(LogType type, const char* fmt, ...){
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    m_logs.push_back({ std::string(buf), type });
    
    // Keep the log from growing forever
    if (m_logs.size() > 500) m_logs.erase(m_logs.begin());
}

// A Helper to launch external apps non-blocking
void DebugView::LaunchAssembler() {
#ifdef _WIN32
    // Get the path of the current running .exe (eater.exe)
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    
    // Remove "eater.exe" to get just the directory
    size_t pos = path.find_last_of("\\/");
    std::string dir = (pos != std::string::npos) ? path.substr(0, pos) : "";

    // Build the full path to the assembler in the same directory
    std::string assembler_path = dir + "\\Assembler.exe";

    // 3. Prepare Win32 Startup structures
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Try to launch the process
    if (CreateProcessA(NULL, (LPSTR)assembler_path.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        // SUCCESS: Update status bar
        m_status_message = "Success: Assembler Studio Launched";
        m_status_timer = 5.0f; // Show for 5 seconds
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        // FAILURE: Update status bar with error
        m_status_message = "Error: Could not find Assembler.exe";
        m_status_timer = 8.0f;
        
        fprintf(stderr, "[ERROR] Could not launch assembler at %s. Error code: %lu\n", 
                assembler_path.c_str(), GetLastError());
    }
#else
    system("./Assembler &");
    m_status_message = "Launched Assembler (Linux)";
    m_status_timer = 3.0f;
#endif
}

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
    if (m_show_cpu)         draw_cpu_window(is_paused, step_request);
    if (m_show_stack)       draw_stack_smart();
    if (m_show_via)         draw_via_window();
    if (m_show_acia)        draw_acia_window();
    if (m_show_ram)         draw_memory_window();
    if (m_show_lcd)         draw_lcd_window();
    if (m_show_rom)         draw_rom_window();
    if (m_show_speed)       draw_speed_control();
    if (m_show_status_bar)  draw_status_bar();
    if (m_show_log)         draw_log_window();
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
            ImGui::MenuItem("Speed Control", nullptr, &m_show_speed);
            ImGui::EndMenu();
        }

        // --- NEW TOOLS MENU ---
        if (ImGui::BeginMenu("Tools")) {
            
            /*// Example 1: Open the ROM file in your default Hex Editor / Text Editor
            if (ImGui::MenuItem("Open ROM File")) {
                LaunchExternal("rom.bin");
            }

            // Example 2: Open System Calculator (Handy for hex math)
            if (ImGui::MenuItem("Calculator")) {
                #ifdef _WIN32
                    LaunchExternal("calc.exe");
                #elif __APPLE__
                    LaunchExternal("-a Calculator");
                #else
                    LaunchExternal("gnome-calculator"); // or kcalc
                #endif
            }
            // Example 3: Open Terminal / Command Prompt
            if (ImGui::MenuItem("Open Terminal")) {
                #ifdef _WIN32
                    LaunchExternal("cmd.exe");
                #else
                    LaunchExternal("x-terminal-emulator");
                #endif
            }*/
           ImGui::MenuItem("Debug View Log", nullptr, &m_show_log);
            // 6502 Assembler studio
            if (ImGui::MenuItem("6502 Assembler Studio")){
                LaunchAssembler();
            }
            

            ImGui::Separator();

            // Example 4: Open a specific custom tool (e.g., your assembler)
            // You can hardcode paths or use relative paths
            if (ImGui::MenuItem("Run ROM Generator")) {
                 // Note: This runs the generator, but won't show output unless you pause to look at the console
                 // or wrap it in a script that pauses.
                 #ifdef _WIN32
                    system("start cmd /k python tools\\rom_build.py"); 
                 #else
                    system("python3 tools/rom_build.py");
                 #endif
            }

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
             ImGui::SameLine();
             if (ImGui::Button("Reset")) {
                if (m_cpu) m_cpu->device_reset();
             }
        } else {
             if (ImGui::Button("Pause")) is_paused = true;
        }

        ImGui::Separator();
        ImGui::Text("Hardware Configuration");

        // 1. Get Current Type
        MachineType current = m_driver->get_machine_type();
        int current_idx = (int)current;
        
        // 2. Define Names
        const char* items[] = { "Schematic 1 (Basic)", "Schematic 2 (Serial)" };
        
        // 3. Draw Combo Box
        if (ImGui::Combo("Motherboard", &current_idx, items, IM_ARRAYSIZE(items))) {
            // 4. If changed, apply new hardware
            m_driver->set_machine_type((MachineType)current_idx);
            
            // Force pause so we don't crash running old code on new hardware
            is_paused = true; 
        }
        
        if (current_idx == 1) {
            ImGui::TextColored(ImVec4(1,1,0,1), "Note: Requires ROM with Serial support!");
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
    ImGui::Begin("LCD Display (U3) - Pixel Perfect");

    // Setup Canvas
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImGui::GetCursorScreenPos();

    // Pixel Tuning Parameters
    float pixelSize = 3.0f;     // Size of each LCD "dot"
    float pixelGap  = 1.0f;     // Space between dots
    float charGap   = 4.0f;     // Space between 5x8 characters

    // LCD Colors (Datasheet-accurate)
    ImU32 colorBg    = IM_COL32(153, 204, 51, 255);  // Yellow-Green background
    ImU32 colorPixel = IM_COL32(30, 40, 10, 255);    // Lit pixel (Dark)
    ImU32 colorDim   = IM_COL32(140, 190, 45, 100);  // Unlit pixel (Slightly darker than BG)

    // 3. Draw the LCD Background Panel
    float totalW = (16 * (5 * (pixelSize + pixelGap) + charGap));
    float totalH = (2 * (8 * (pixelSize + pixelGap) + charGap));
    drawList->AddRectFilled(startPos, ImVec2(startPos.x + totalW, startPos.y + totalH), colorBg);

    // 4. Render 2 Rows of 16 Characters
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 16; col++) {
            
            // Get DDRAM address and character code
            uint8_t addr = (row == 0) ? (0x00 + col) : (0x40 + col);
            uint8_t charCode = m_lcd->get_ddram()[addr];
            uint8_t cursorAC = m_lcd->get_cursor_addr();

            // Fetch bit pattern (Check if CGRAM or CGROM)
            const uint8_t* pattern;
            if (charCode < 0x08) {
                pattern = &m_lcd->get_cgram()[charCode * 8]; // User custom char
            } else {
                pattern = nhd_0216k1z::CGROM_A00[charCode];   // Standard ROM char
            }

            // Draw the 5x8 Matrix for this character
            for (int y = 0; y < 8; y++) {
                uint8_t rowBits = pattern[y];

                // Handle Cursor Logic (Blinking or Underscore)
                bool isCursorPos = (addr == cursorAC);
                bool blinkOn = m_lcd->is_blink_on() && ((int)(ImGui::GetTime() * 2.0f) % 2 == 0);
                
                for (int x = 0; x < 5; x++) {
                    // Check if pixel is lit (Bit 4 is the leftmost dot)
                    bool bitLit = (rowBits >> (4 - x)) & 0x01;
                    
                    // Logic for Cursor: OR with the character pattern
                    if (isCursorPos && m_lcd->is_cursor_on()) {
                        if (m_lcd->is_blink_on()) {
                            if (blinkOn) bitLit = true; // Full block blink
                        } else if (y == 7) {
                            bitLit = true; // Underscore on 8th line [cite: 19, 26]
                        }
                    }

                    // Calculate screen position for this specific dot
                    ImVec2 p1 = {
                        startPos.x + (col * (5 * (pixelSize + pixelGap) + charGap)) + (x * (pixelSize + pixelGap)),
                        startPos.y + (row * (8 * (pixelSize + pixelGap) + charGap)) + (y * (pixelSize + pixelGap))
                    };
                    ImVec2 p2 = { p1.x + pixelSize, p1.y + pixelSize };

                    drawList->AddRectFilled(p1, p2, bitLit ? colorPixel : colorDim);
                }
            }
        }
    }

    // Move ImGui cursor past the drawing area so other UI elements don't overlap
    ImGui::Dummy(ImVec2(totalW, totalH));
    ImGui::End();
}

void DebugView::draw_speed_control() {
    ImGui::Begin("Clock Control");

    ImGui::Text("Target Speed:");
    
    // Logarithmic slider feels better for speed (1Hz to 1MHz)
    // We use a float for the slider, then cast to int
    static float speed_log = 6.0f; // 10^6 = 1MHz
    
    if (ImGui::SliderFloat("##speed", &speed_log, 0.0f, 6.0f, "10^%.1f Hz")) {
        // Convert Log10 back to Integer Hz
        // 0.0 -> 1 Hz
        // 6.0 -> 1,000,000 Hz
        m_target_hz = (int)pow(10.0f, speed_log);
    }

     
    // Quick Presets
    if (ImGui::Button("1 Hz"))    { m_target_hz = 1;       speed_log = 0.0f; } ImGui::SameLine();
    if (ImGui::Button("10 Hz"))   { m_target_hz = 10;      speed_log = 1.0f; } ImGui::SameLine();
    if (ImGui::Button("1 kHz"))   { m_target_hz = 1000;    speed_log = 3.0f; } ImGui::SameLine();
    if (ImGui::Button("1 MHz"))   { m_target_hz = 1000000; speed_log = 6.0f; }

    ImGui::Separator();
    ImGui::Text("Current Target: %d Hz", m_target_hz);

    ImGui::End();
}

void DebugView::draw_status_bar() {
    // Position at the very bottom of the viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - ImGui::GetFrameHeight()));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, ImGui::GetFrameHeight()));

    // Use transparent background and no decorations
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
    
    // Set a subtle background color (darker than your clear color)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); 
    
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        ImGui::TextUnformatted(m_status_message.c_str());
        ImGui::End();
    }
    ImGui::PopStyleColor();

    // Handle the timer logic
    if (m_status_timer > 0) {
        m_status_timer -= ImGui::GetIO().DeltaTime;
        if (m_status_timer <= 0) {
            m_status_message = "Ready";
        }
    }
}

void DebugView::draw_log_window() {
    if (!m_show_log) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("System Log", &m_show_log)){
        if(ImGui::Button("Clear")) m_logs.clear();
        ImGui::SameLine();
        if (ImGui::Button("Copy to Clipboard")) { /* ... */}

        ImGui::Separator();

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& entry : m_logs) {
            ImVec4 color = ImVec4(1,1,1,1); // Default White
            if (entry.type == LOG_CPU)   color = ImVec4(0.7f, 0.7f, 1.0f, 1.0f); // Soft Blue
            if (entry.type == LOG_IO)    color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f); // Cyan
            if (entry.type == LOG_ERROR) color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Red

            ImGui::TextColored(color, "%s", entry.text.c_str());
        }

        // Auto-Scroll logic
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        
        ImGui::EndChild();
    }
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