#ifndef DEBUG_VIEW_H
#define DEBUG_VIEW_H

#include <imgui.h>
#include <cstdint>

// ============================================================================
// Forward Declarations
// ============================================================================
// We tell the compiler "These classes exist" without needing their files yet.
// As you build them, you just uncomment/add them here or include their headers.
class m6502_p; 
class w65c22;       // Placeholder for U5
class w65c51;       // Placeholder for U7
class mb_driver;    // Placeholder for your mainboard/driver
class nhd_0216k1z;

class DebugView {
public:
    // Constructor: We pass pointers to hardware. 
    // If a device isn't ready, pass 'nullptr'.
    DebugView(mb_driver* driver);

    // Main Draw Loop (Called every frame by Renderer)
    void draw(bool& is_paused, bool& step_request);

private:
    // ----- Hardware Pointers -----
    mb_driver* m_driver;
    m6502_p* m_cpu;
    w65c22* m_via;
    w65c51* m_acia;
    nhd_0216k1z* m_lcd;

    // UI Buffers
    char m_rom_path[256] = "rom.bin";
    char m_status_msg[128] = "System Ready";

    // --- Window Visibility Flags ---
    bool m_show_cpu     = true;
    bool m_show_stack   = true;
    bool m_show_via     = false;
    bool m_show_acia    = false;
    bool m_show_ram     = true;  // Generic Memory Viewer
    bool m_show_lcd     = true;
    bool m_show_rom     = false;

    // --- Helper Functions ---
    void draw_menu_bar(bool& is_paused, bool& step_request);
    
    // Renders the "00 01 02..." hex header for memory views
    void draw_byte_header(int columns = 16, const char* padding = "      ");

    // --- Sub-Windows ---
    void draw_cpu_window(bool& is_paused, bool& step_request);
    // Specific Stack Visualizer
    void draw_stack_smart();
    void draw_via_window();     // Skeleton
    void draw_acia_window();    // Skeleton
    void draw_memory_window();  // Skeleton
    void draw_lcd_window();
    void draw_rom_window();
};

#endif // DEBUG_VIEW_H