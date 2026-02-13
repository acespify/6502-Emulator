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
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cerr << "[System] Initializing Emulator..." << std::endl;

    // 1. Setup UI
    Renderer renderer;
    if (!renderer.init(1920, 1080, "Ben Eater 6502 Emulator")) {
        std::cerr << "[System] Renderer Init Failed!" << std::endl;
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
    const int UI_FPS = 60;
    const auto ui_frame_duration = std::chrono::microseconds(1000000 / UI_FPS);

    std::cerr << "Starting Main Loop..." << std::endl;

    // 4. Main Loop
    while (!renderer.should_close()) {
        auto frame_start = std::chrono::steady_clock::now();

        renderer.begin_frame();

        // 1. Get Target Speed from UI
        int target_hz = debugger.get_target_hz();
        
        // 2. Calculate cycles per frame
        // If 1MHz at 60FPS -> 16666 cycles/frame
        // If 10Hz  at 60FPS -> 0.16 cycles/frame (Accumulate fractions!)
        
        static float cycle_accumulator = 0.0f;
        float cycles_per_frame = (float)target_hz / (float)UI_FPS;

        // CPU Execution Logic
        if (!is_paused || step_req) {
            
            // Handle slow speeds (< 60Hz) by accumulating partial cycles
            cycle_accumulator += cycles_per_frame;
            
            int cycles_to_run = (int)cycle_accumulator;
            
            if (step_req) {
                cycles_to_run = 1; // Step overrides speed
                cycle_accumulator = 0;
            }

            if (cycles_to_run > 0) {
                computer.run(cycles_to_run);
                cycle_accumulator -= cycles_to_run; // Keep remainder
            }

            step_req = false;
        }

        // Draw UI
        debugger.draw(is_paused, step_req);
        
        // TODO: Draw the LCD Window here later!
        // debugger.draw_lcd_window(computer.get_lcd()); 

        renderer.end_frame();

        auto frame_end = std::chrono::steady_clock::now();
        auto elapsed = frame_end - frame_start;

        if (elapsed < ui_frame_duration) {
            std::this_thread::sleep_for(ui_frame_duration - elapsed);
        }
    }
    std::cerr << "Main Loop Exited." << std::endl;
    renderer.shutdown();
    return 0;
}