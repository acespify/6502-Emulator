#ifndef RENDERER_H
#define RENDERER_H

// Adjust these includes based on where your vendor files are
// You might need <GLFW/glfw3.h> or "glfw3.h" depending on your include paths
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <cstdio>

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initialize GLFW and ImGui
    bool init(int width, int height, const char* title);

    // Explicit shutdow method
    void shutdown();

    // Start a new frame (call this at start of loop)
    void begin_frame();
    
    // Render and Swap buffers (call this at end of loop)
    void end_frame();

    // Check if user clicked the "X" button
    bool should_close();

private:
    GLFWwindow* m_window;
    bool m_is_active;
    int m_base_width = 1280; // Default, will be overwritten in init
};

#endif // RENDERER_H