#include "renderer.h"

Renderer::Renderer() : m_window(nullptr), m_is_active(false) {}

Renderer::~Renderer() {
    shutdown();
}

void Renderer::shutdown() {
    // 1. Cleanup ImGui (Only if it was successfully initialized)
    if (m_is_active) {
        
        // A. Standard Backend Shutdown
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyPlatformWindows(); // added

        // B. Context Destruction with Safety Net
        if (ImGui::GetCurrentContext()) {
            
            // --- SAFETY NET ---
            // We manually clear the viewport data. This satisfies the assertion 
            // at imgui.cpp line 4429 regarding "DestroyPlatformWindows".
            ImGuiViewport* main_viewport = ImGui::GetMainViewport();
            if (main_viewport) {
                main_viewport->PlatformUserData = nullptr;
                main_viewport->RendererUserData = nullptr;
                main_viewport->PlatformHandle = nullptr;
            }
            // ------------------
            
            ImGui::DestroyContext();
             
        }
    }

    // 2. Cleanup GLFW Window (Only if window exists)
    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
    m_is_active = false; // Mark as dead so we don't try again
}

bool Renderer::init(int width, int height, const char* title) {
    if (!glfwInit()) {
        printf("ERROR: Failed to initialize GLFW\n");
        return false;
    }

    // Basic OpenGL 3.0+ Setup
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // --- DPI AWARENESS (New) ---
    // Allow the window to scale automatically on OSs that support it (Windows 10/11, macOS)
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!m_window) {
        printf("ERROR: Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable V-Sync

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // --- GET MONITOR DPI SCALE ---
    float x_scale = 1.0f, y_scale = 1.0f;
    glfwGetWindowContentScale(m_window, &x_scale, &y_scale);
    
    // Use the larger scale factor (usually x and y are the same)
    float dpi_scale = (x_scale > y_scale ? x_scale : y_scale);
    
    printf("[SYSTEM] Monitor DPI Scale: %.2f\n", dpi_scale);

    // --- APPLY SCALING TO STYLE ---
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpi_scale); // Scales padding, rounding, spacing, etc.

    // --- LOAD SCALED FONT ---
    // ImGui's default font is bitmap and doesn't scale well. 
    // We should load a TrueType font (like Arial or Roboto) if available.
    // If not, we can scale the default font, but it might look blurry.
    
    // Attempt 1: Load a system font (Windows example)
    // 13.0f is a base size. We multiply by dpi_scale (e.g., 13 * 1.5 = 19.5px on 150% scale)
    float base_font_size = 16.0f;
    float scaled_size = base_font_size * dpi_scale;

    //ImFont* font = nullptr; not used
    if (io.Fonts) {
        // Clear default font to ensure we use the custom one
        io.Fonts->Clear();

        // High-Quality Font Settings
        ImFontConfig config;
        config.OversampleH = 3; // Horizontal anti-aliasing (Sharper text)
        config.OversampleV = 3; // Vertical anti-aliasing
        
        // OPTION 1: Consolas (Monospaced - Recommended for Hex Dumps)
        // Check if file exists first to avoid crash
        if (FILE* f = fopen("C:\\Windows\\Fonts\\consola.ttf", "rb")) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", scaled_size, &config);
        }
        // OPTION 2: Segoe UI (Modern Windows Look - Good for Menus)
        else if (FILE* f = fopen("C:\\Windows\\Fonts\\segoeui.ttf", "rb")) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", scaled_size, &config);
        }
        // OPTION 3: Arial (Safe Fallback)
        else if (FILE* f = fopen("C:\\Windows\\Fonts\\arial.ttf", "rb")) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", scaled_size, &config);
        }
        else {
            // Last Resort: Built-in Pixel Font (Scaled up)
            ImFontConfig pixel_config;
            pixel_config.SizePixels = scaled_size;
            io.Fonts->AddFontDefault(&pixel_config);
        }
        
        // Re-build font atlas texture
        io.Fonts->Build();
    }

    // Init ImGui Backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    m_is_active = true;
    return true;
}

void Renderer::begin_frame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Renderer::end_frame() {
    if (!m_is_active) return;
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    
    // Clear Screen (Grey background)
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
}

bool Renderer::should_close() {
    if (!m_window) return true;
    return glfwWindowShouldClose(m_window);
}