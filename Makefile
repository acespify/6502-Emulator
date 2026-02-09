# =========================================================================
# Project: Eater 6502 Emulator
# =========================================================================

TARGET_EXEC := eater.exe
BUILD_DIR   := ./build
SRC_DIR     := ./src
VENDOR_DIR  := ./vendor

CXX         := g++
# Added -I./src to the root flags so you can do #include "driver/mb_driver.h"
CXXFLAGS    := -std=c++17 -O2 -g -Wall -D_WIN32_WINNT=0x0A00 -I$(SRC_DIR)

# -------------------------------------------------------------------------
# 1. Bulletproof Source Discovery (Recursive Wildcard)
# -------------------------------------------------------------------------
# This function recursively searches a folder for files matching a pattern
# It works on ALL systems (Windows/Linux/Mac) without external tools.
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Find ALL .cpp files in src/ (and subfolders)
ALL_PROJECT_SRCS := $(call rwildcard,$(SRC_DIR),*.cpp)

# Filter out the generator tool
PROJECT_SRCS := $(filter-out %rom_generator.cpp, $(ALL_PROJECT_SRCS))

# Vendor Sources (ImGui)
IMGUI_DIR   := $(VENDOR_DIR)/imgui
VENDOR_SRCS := $(IMGUI_DIR)/imgui.cpp \
               $(IMGUI_DIR)/imgui_draw.cpp \
               $(IMGUI_DIR)/imgui_tables.cpp \
               $(IMGUI_DIR)/imgui_widgets.cpp \
               $(IMGUI_DIR)/imgui_demo.cpp \
               $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
               $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

# --- KEY CHANGE: Define Object Lists Separately ---
PROJECT_OBJS := $(PROJECT_SRCS:%=$(BUILD_DIR)/obj/%.o)
VENDOR_OBJS  := $(VENDOR_SRCS:%=$(BUILD_DIR)/obj/%.o)

# Combine lists
OBJS := $(PROJECT_SRCS:%=$(BUILD_DIR)/obj/%.o) $(VENDOR_SRCS:%=$(BUILD_DIR)/obj/%.o)
DEPS := $(OBJS:.o=.d)

# -------------------------------------------------------------------------
# 2. Includes & Libraries
# -------------------------------------------------------------------------
# Note: We do NOT include source folders here anymore. 
# You should include files relative to 'src', e.g. #include "devices/cpu/m6502.h"
INC_FLAGS := -I$(VENDOR_DIR)/imgui \
             -I$(VENDOR_DIR)/imgui/backends \
             -I$(VENDOR_DIR)/GLFW/include \
             -I$(VENDOR_DIR)/glfw/include \
             -I$(VENDOR_DIR) \
             -I$(VENDOR_DIR)/stb_image \
             -I$(VENDOR_DIR)/asio/include

LDFLAGS := -L$(VENDOR_DIR)/GLFW/lib -lglfw3 -lgdi32 -lopengl32 -lws2_32 -lmswsock -static-libgcc -static-libstdc++

# -------------------------------------------------------------------------
# 3. Build Rules
# -------------------------------------------------------------------------

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	@echo "Linking $(TARGET_EXEC)..."
	@mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build Success! Run: $(BUILD_DIR)/$(TARGET_EXEC)"

# Compile C++
$(BUILD_DIR)/obj/%.cpp.o: %.cpp
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC_FLAGS) -MMD -MP -c $< -o $@

.PHONY: clean clean-all info

# -------------------------------------------------------------------------
# 4. Cleaning Rules 
# -------------------------------------------------------------------------


# 'make clean' -> Removes ONLY project objects + executable. Keeps ImGui compiled.
clean:
	@echo "Cleaning project files (keeping vendor objects)..."
	@rm -f $(PROJECT_OBJS)
	@rm -f $(PROJECT_OBJS:.o=.d)
	@rm -f $(BUILD_DIR)/$(TARGET_EXEC)

clean-all:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

info:
	@echo "--------------------------------------"
	@echo "Detected Sources:"
	@echo $(PROJECT_SRCS)
	@echo "--------------------------------------"
