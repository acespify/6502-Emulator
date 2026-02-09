#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>

// UI Includes
#include "ui/renderer.h"
#include "ui/views/debug_view.h"

// Hardware Includes
#include "driver/mainboard.h"

// ============================================================================
// 4. MAIN ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    std::cout << "[System] Initializing Emulator..." << std::endl;

    // 1. Setup UI
    Renderer renderer;
    if (!renderer.init(1920, 1080, "Ben Eater 6502 Emulator")) {
        return -1;
    }

    // 2. Setup Machine (The Driver)
    // This runs the internal init(), loads the ROM, and wires the schematic.
    mb_driver computer;
    computer.init();
    computer.reset();

    // 3. Setup Debugger
    // We pass the pointers to the specific chips so the UI can "peek" at them.
    bool is_paused = true; 
    bool step_req = false;
    
    // Note: We now pass the LCD pointer too! You might need to update DebugView constructor later.
    DebugView debugger(&computer);

    // Timing Variables
    // Target: 60FPS (16.66ms per frame)
    const auto target_frame_duration = std::chrono::microseconds(16667);

    // 4. Main Loop
    while (!renderer.should_close()) {
        auto frame_start = std::chrono::steady_clock::now();

        renderer.begin_frame();

        // CPU Execution Logic
        if (!is_paused || step_req) {
            // Speed: 1MHz = 1,000,000 cycles / 60 FPS = ~16667 cycles per frame
            int cycles = step_req ? 1 : 16667;
            
            computer.run(cycles);
            
            step_req = false;
        }

        // Draw UI
        debugger.draw(is_paused, step_req);
        
        // TODO: Draw the LCD Window here later!
        // debugger.draw_lcd_window(computer.get_lcd()); 

        renderer.end_frame();

        auto frame_end = std::chrono::steady_clock::now();
        auto elapsed = frame_end - frame_start;

        if (elapsed < target_frame_duration) {
            std::this_thread::sleep_for(target_frame_duration - elapsed);
        }
    }

    renderer.shutdown();
    return 0;
}