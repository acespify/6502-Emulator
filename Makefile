# ==========================================
# EATER 6502 EMULATOR - STATIC BUILD
# ==========================================

APP_NAME      := eater.exe
CXX           := g++
CXXFLAGS      := -std=c++17 -g -Wall -Wextra -D_WIN32_WINNT=0x0A00

# Directories
BUILD_DIR     := build
SRC_DIR       := src
LIB_SRC_DIR   := libs
ASSETS_DIR    := assets
VENDOR_DIR    := vendor
IMGUI_DIR     := $(VENDOR_DIR)/imgui

.DEFAULT_GOAL := all

.PHONY: all run clean clean-all info copy-dlls copy-assets copy-tools bump_build

# ==========================================
# VERSIONING SYSTEM
# ==========================================

VERSION      := 0.1.0
GIT_HASH     := $(shell git rev-parse --short HEAD 2>/dev/null || echo "nogit")
BUILD_DATE   := $(shell date +"%Y-%m-%d %H:%M:%S")
BUILD_FILE   := build_number.txt
BUILD_NUMBER := $(shell [ -f $(BUILD_FILE) ] && cat $(BUILD_FILE) || echo 0)

# Calculate the next number safely, avoiding Windows shell math bugs
NEXT_BUILD_NUMBER := $(shell expr $(BUILD_NUMBER) + 1)

VERSION_HEADER := $(SRC_DIR)/version.h

# Generate version.h ONLY if it doesn't exist (Preserves incremental builds!)
$(VERSION_HEADER): $(wildcard .git/HEAD) $(wildcard .git/index) $(BUILD_FILE)
	@echo "Generating version.h..." 
	@echo "#pragma once" > $(VERSION_HEADER) 
	@echo "#define APP_VERSION \"$(VERSION)\"" >> $(VERSION_HEADER) 
	@echo "#define APP_GIT_HASH \"$(GIT_HASH)\"" >> $(VERSION_HEADER) 
	@echo "#define APP_BUILD_DATE \"$(BUILD_DATE)\"" >> $(VERSION_HEADER) 
	@echo "#define APP_BUILD_NUMBER \"$(BUILD_NUMBER)\"" >> $(VERSION_HEADER)

# Run `make bump_build` to manually increment and rebuild the header
bump_build:
	@echo $(NEXT_BUILD_NUMBER) > $(BUILD_FILE)
	@rm -f $(VERSION_HEADER)
	@$(MAKE) $(VERSION_HEADER) --no-print-directory

# ==========================================
# INCLUDE PATHS
# ==========================================

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

ifneq ("$(wildcard $(RESOURCE_FILE))","")
    RESOURCE_OBJ := $(BUILD_DIR)/obj/eater.res
endif

# --- LINKING ---
LDFLAGS       := -L$(VENDOR_DIR)/GLFW/lib
LIBS          := -lglfw3 -lopengl32 -lgdi32 -lws2_32 -lmswsock -limm32 -lwinmm -static-libgcc -static-libstdc++

# ==========================================
# SOURCE DISCOVERY
# ==========================================
# Recursive wildcard function
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
                    $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp \
					$(IMGUI_DIR)/ImGuiFileDialog.cpp

# 3. Object Lists
PROJECT_OBJS := $(PROJECT_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/obj/%.o)
VENDOR_OBJS  := $(VENDOR_SRCS:$(VENDOR_DIR)/%.cpp=$(BUILD_DIR)/vendor/%.o)

# Combined Objects
OBJS := $(PROJECT_OBJS) $(VENDOR_OBJS)
DEPS := $(OBJS:.o=.d)

# ==========================================
# TARGETS
# ==========================================

all: bump_build $(VERSION_HEADER) $(BUILD_DIR)/$(APP_NAME) copy-dlls copy-assets copy-tools

# Link Final Executable
$(BUILD_DIR)/$(APP_NAME): $(OBJS) $(RESOURCE_OBJ)
	@echo "Linking $(APP_NAME)..."
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build Success! Run: $(BUILD_DIR)/$(APP_NAME)"

# ==========================================
# FILE DEPLOYMENT (Cross-Platform Safe)
# ==========================================

HAS_DLLS   := $(wildcard $(LIB_SRC_DIR)/*.dll)
HAS_ASSETS := $(wildcard $(ASSETS_DIR)/*)
HAS_TOOLS  := $(wildcard tools/Assembler/Assembler.exe)

copy-dlls:
ifneq ($(strip $(HAS_DLLS)),)
	@echo "Deploying DLLs..."
	@mkdir -p $(BUILD_DIR)
	@cp $(LIB_SRC_DIR)/*.dll $(BUILD_DIR)/ 2>/dev/null || true
endif

copy-assets:
ifneq ($(strip $(HAS_ASSETS)),)
	@echo "Deploying Assets..."
	@mkdir -p $(BUILD_DIR)/assets
	@cp -r $(ASSETS_DIR)/* $(BUILD_DIR)/assets/ 2>/dev/null || true
endif

copy-tools:
ifneq ($(strip $(HAS_TOOLS)),)
	@echo "Deploying Assembler tool..."
	@mkdir -p $(BUILD_DIR)
	@cp tools/Assembler/Assembler.exe $(BUILD_DIR)/ 2>/dev/null || true
endif

# ==========================================
# COMPILATION RULES
# ==========================================

# Compile Project C++ Files
$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.cpp $(VERSION_HEADER)
	@echo "Compiling $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Compile Vendor C++ Files (Added -MMD -MP to generate .d files)
$(BUILD_DIR)/vendor/%.o: $(VENDOR_DIR)/%.cpp
	@echo "Compiling Vendor $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Compile Resource File
$(BUILD_DIR)/obj/%.res: $(SRC_DIR)/%.rc
	@echo "Compiling Resource $<"
	@mkdir -p $(dir $@)
	@$(WINDRES) $< -O coff -o $@

# ==========================================
# UTILITY COMMANDS
# ==========================================

run: all
	@echo "Running..."
	@./$(BUILD_DIR)/$(APP_NAME)

clean:
	@echo "Cleaning Project Files..."
	@rm -rf $(BUILD_DIR)/obj
	@rm -f $(BUILD_DIR)/$(APP_NAME)

clean-all:
	@echo "Cleaning Everything..."
	@rm -rf $(BUILD_DIR)

info:
	@echo "Project Sources: $(PROJECT_SRCS)"
	@echo "Vendor Sources: $(VENDOR_SRCS)"

# Include compiler-generated dependency files
-include $(DEPS)