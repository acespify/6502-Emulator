# ==========================================
# EATER 6502 EMULATOR - STATIC BUILD
# ==========================================

APP_NAME      := eater.exe
CXX           := g++
CXXFLAGS      := -std=c++17 -g -Wall -Wextra -D_WIN32_WINNT=0x0A00

# Directories
BUILD_DIR     := build
SRC_DIR       := src
LIB_SRC_DIR	  := libs
ASSETS_DIR	  := assets
VENDOR_DIR    := vendor
IMGUI_DIR     := $(VENDOR_DIR)/imgui

# Include Paths (Adjusted for your structure)
# Note: We include SRC_DIR so #include "devices/cpu/m6502.h" works
INCLUDES      := -I$(SRC_DIR) \
                 -I$(IMGUI_DIR) \
                 -I$(IMGUI_DIR)/backends \
                 -I$(VENDOR_DIR)/GLFW/include \
                 -I$(VENDOR_DIR)/glfw/include \
                 -I$(VENDOR_DIR) \
                 -I$(VENDOR_DIR)/stb_image \
                 -I$(VENDOR_DIR)/asio/include

# --- RESOURCE COMPILER ---
WINDRES       := windres
RESOURCE_FILE := $(SRC_DIR)/eater.rc
# Check if resource file exists before trying to compile it
ifneq ("$(wildcard $(RESOURCE_FILE))","")
    RESOURCE_OBJ := $(BUILD_DIR)/obj/eater.res
endif

# --- LINKING ---
# 1. Library Paths
LDFLAGS       := -L$(VENDOR_DIR)/GLFW/lib

# 2. Libraries (Static Link)
# -static flags ensure we don't need MinGW DLLs at runtime
LIBS          := -lglfw3 -lopengl32 -lgdi32 -lws2_32 -lmswsock -limm32 -lwinmm -static-libgcc -static-libstdc++ #-mwindows

# -------------------------------------------------------------------------
# SOURCE DISCOVERY
# -------------------------------------------------------------------------
# Recursive wildcard function (Works on Windows/Linux)
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# 1. Project Sources (Filter out the generator tool)
ALL_PROJECT_SRCS := $(call rwildcard,$(SRC_DIR),*.cpp)
PROJECT_SRCS     := $(filter-out %rom_generator.cpp, $(ALL_PROJECT_SRCS))

# 2. Vendor Sources (ImGui)
VENDOR_SRCS      := $(IMGUI_DIR)/imgui.cpp \
                    $(IMGUI_DIR)/imgui_draw.cpp \
                    $(IMGUI_DIR)/imgui_tables.cpp \
                    $(IMGUI_DIR)/imgui_widgets.cpp \
                    $(IMGUI_DIR)/imgui_demo.cpp \
                    $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
                    $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

# 3. Object Lists
PROJECT_OBJS := $(PROJECT_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/obj/%.o)
VENDOR_OBJS  := $(VENDOR_SRCS:$(VENDOR_DIR)/%.cpp=$(BUILD_DIR)/vendor/%.o)

# Combined Objects
OBJS := $(PROJECT_OBJS) $(VENDOR_OBJS)
DEPS := $(OBJS:.o=.d)

# ==========================================
# TARGETS
# ==========================================

all: $(BUILD_DIR)/$(APP_NAME) copy-dlls copy-assets copy-tools

# Link Final Executable
$(BUILD_DIR)/$(APP_NAME): $(OBJS) $(RESOURCE_OBJ)
	@echo "Linking $(APP_NAME)..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build Success! Run: $(BUILD_DIR)/$(APP_NAME)"

# Copy DLLs Target
copy-dlls:
	@echo "Copying DLLs from $(LIB_SRC_DIR) to $(BUILD_DIR)..."
	@mkdir -p $(BUILD_DIR)
	@if [ -d "$(LIB_SRC_DIR)" ]; then cp $(LIB_SRC_DIR)/*.dll $(BUILD_DIR)/ 2>/dev/null || :; fi
	@echo "DLL Deployment Complete."

# Copy Assets
copy-assets:
	@echo "Deploying Assets..."
	@mkdir -p $(BUILD_DIR)/assets
	@if [ -d "$(ASSETS_DIR)" ]; then cp -r $(ASSETS_DIR)/* $(BUILD_DIR)/assets/ 2>/dev/null || :; fi
	@echo "Assets have been Deployed..."

# Copy Tools
copy-tools:
	@echo "Deploying Assembler..."
	@mkdir -p $(BUILD_DIR)
	@if [ -f "tools/Assembler/Assembler.exe" ]; then cp "tools/Assembler/Assembler.exe" $(BUILD_DIR)/; fi
	@echo "Tools have been Deployed..."

# Compile Project C++ Files
$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.cpp
	@echo "Compiling $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Compile Vendor C++ Files (ImGui)
$(BUILD_DIR)/vendor/%.o: $(VENDOR_DIR)/%.cpp
	@echo "Compiling Vendor $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile Resource File (if it exists)
$(BUILD_DIR)/obj/%.res: $(SRC_DIR)/%.rc
	@echo "Compiling Resource $<"
	@mkdir -p $(dir $@)
	@$(WINDRES) $< -O coff -o $@

# --- UTILITY COMMANDS ---

# Run the emulator
run: all
	@echo "Running..."
	@./$(BUILD_DIR)/$(APP_NAME)

# Clean only project files
clean:
	@echo "Cleaning Project Files..."
	@rm -rf $(BUILD_DIR)/obj
	@rm -f $(BUILD_DIR)/$(APP_NAME)

# Clean everything (including ImGui)
clean-all:
	@echo "Cleaning Everything..."
	@rm -rf $(BUILD_DIR)

# Debug Info
info:
	@echo "Project Sources: $(PROJECT_SRCS)"
	@echo "Vendor Sources: $(VENDOR_SRCS)"

.PHONY: all run clean clean-all info

-include $(DEPS)