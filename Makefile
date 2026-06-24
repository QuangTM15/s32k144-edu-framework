# ============================================================
# EduFramework_v2 Makefile
# Target  : NXP S32K144 / MaaZEDU
# Purpose : Build EduFramework static library package
#
# Output:
#   eduframework/include/*.h
#   eduframework/lib/libeduframework.a
#
# Environment:
#   Windows PowerShell
#   GNU Make
#   S32 Design Studio GCC 10.2
# ============================================================


# ============================================================
# Toolchain Configuration
# ============================================================

# GNU Arm Embedded Toolchain directory from S32 Design Studio.
# This can be overridden from command line if needed:
#   make TOOLCHAIN_DIR=C:/path/to/toolchain/bin
TOOLCHAIN_DIR ?= C:/NXP/S32DS.3.5/S32DS/build_tools/gcc_v10.2/gcc-10.2-arm32-eabi/bin

# C compiler.
CC := $(TOOLCHAIN_DIR)/arm-none-eabi-gcc.exe

# Static library archiver.
AR := $(TOOLCHAIN_DIR)/arm-none-eabi-ar.exe

# Size utility for optional inspection.
SIZE := $(TOOLCHAIN_DIR)/arm-none-eabi-size.exe


# ============================================================
# Project Directories
# ============================================================

# Main framework source directory.
FIRMWARE_DIR := firmware

# S32K144 device header directory.
S32DS_INCLUDE_DIR := s32ds/EduFramework/include

# Temporary build directory.
BUILD_DIR := build

# Object file directory.
OBJ_DIR := $(BUILD_DIR)/obj

# Packaged framework output directory.
PACKAGE_DIR := eduframework

# Public exported header directory.
PACKAGE_INC_DIR := $(PACKAGE_DIR)/include

# Static library output directory.
PACKAGE_LIB_DIR := $(PACKAGE_DIR)/lib

# Final static library output file.
TARGET_LIB := $(PACKAGE_LIB_DIR)/libeduframework.a


# ============================================================
# Automatic Source Discovery
# ============================================================

# Find all .c files under firmware/.
# These files are compiled into object files.
C_SOURCES := $(shell powershell -NoProfile -Command "Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -Filter *.c | ForEach-Object { $$_.FullName.Replace((Get-Location).Path + '\','').Replace('\','/') }")

# Find all .h files under firmware/.
# These files are exported into eduframework/include/.
H_SOURCES := $(shell powershell -NoProfile -Command "Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -Filter *.h | ForEach-Object { $$_.FullName.Replace((Get-Location).Path + '\','').Replace('\','/') }")

# Find all include folders named inc under firmware/.
# Every inc folder is added to compiler include paths.
INC_DIRS := $(shell powershell -NoProfile -Command "Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -Directory -Filter inc | ForEach-Object { $$_.FullName.Replace((Get-Location).Path + '\','').Replace('\','/') }")

# Convert firmware/x/y/file.c into build/obj/firmware/x/y/file.o.
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SOURCES))

# Convert include folder list into -I compiler flags.
INCLUDES := $(addprefix -I,$(INC_DIRS))

# Add S32K144 device headers from S32DS project.
INCLUDES += -I$(S32DS_INCLUDE_DIR)


# ============================================================
# Compiler Configuration
# ============================================================

# Cortex-M4F CPU options for S32K144.
CPU_FLAGS := \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard

# Device macro expected by S32K144 headers.
DEFINES := \
	-DCPU_S32K144HFT0VLLT

# Common C compiler flags.
CFLAGS := \
	$(CPU_FLAGS) \
	$(DEFINES) \
	$(INCLUDES) \
	-std=c99 \
	-O0 \
	-g3 \
	-ffunction-sections \
	-fdata-sections \
	-Wall \
	-Wextra \
	-Wno-unused-parameter

# Static library archiver flags.
ARFLAGS := rcs


# ============================================================
# Default Target
# ============================================================

# make
#
# Build complete package:
#   - libeduframework.a
#   - exported public headers
.PHONY: all
all: package


# ============================================================
# Package Target
# ============================================================

# make package
#
# Build the static library and export all public headers.
# This is the main target used before committing/releasing
# a new framework package.
.PHONY: package
package: lib headers
	@echo.
	@echo [DONE] EduFramework package generated.
	@echo [LIB ] $(TARGET_LIB)
	@echo [INC ] $(PACKAGE_INC_DIR)


# ============================================================
# Library Target
# ============================================================

# make lib
#
# Compile all firmware/*.c files and archive them into
# eduframework/lib/libeduframework.a.
.PHONY: lib
lib: $(TARGET_LIB)

# Create static library from all object files.
$(TARGET_LIB): $(OBJECTS)
	@echo.
	@echo [AR  ] $@
	@if not exist "$(subst /,\,$(PACKAGE_LIB_DIR))" mkdir "$(subst /,\,$(PACKAGE_LIB_DIR))"
	$(AR) $(ARFLAGS) $@ $(OBJECTS)


# ============================================================
# Compile Rule
# ============================================================

# Compile each .c file into a matching .o file under build/obj/.
# The original source tree layout is preserved inside build/obj/
# to avoid object-name collisions.
$(OBJ_DIR)/%.o: %.c
	@echo [CC  ] $<
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Header Export Target
# ============================================================

# make headers
#
# Export all firmware/*.h files into eduframework/include/.
#
# Note:
#   This exports headers into a flat include directory.
#   Current project header names are unique, so this is OK.
#   If future HAL/sensor modules introduce duplicate header
#   names, header export should be changed to preserve folders.
.PHONY: headers
headers:
	@echo.
	@echo [HDR ] Exporting public headers...
	@if not exist "$(subst /,\,$(PACKAGE_INC_DIR))" mkdir "$(subst /,\,$(PACKAGE_INC_DIR))"
	powershell -NoProfile -Command "Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -Filter *.h | Copy-Item -Destination '$(PACKAGE_INC_DIR)' -Force"


# ============================================================
# Clean Targets
# ============================================================

# make clean
#
# Remove only temporary build artifacts.
# This does NOT delete eduframework/include or libeduframework.a.
.PHONY: clean
clean:
	@echo [CLEAN] $(BUILD_DIR)
	@if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"


# make distclean
#
# Remove temporary build artifacts and generated package files.
# Use this when a completely fresh exported package is needed.
.PHONY: distclean
distclean: clean
	@echo [CLEAN] $(TARGET_LIB)
	@if exist "$(subst /,\,$(TARGET_LIB))" del /Q "$(subst /,\,$(TARGET_LIB))"
	@echo [CLEAN] $(PACKAGE_INC_DIR)/*.h
	@if exist "$(subst /,\,$(PACKAGE_INC_DIR))\*.h" del /Q "$(subst /,\,$(PACKAGE_INC_DIR))\*.h"


# make rebuild
#
# Fully clean and rebuild the framework package.
.PHONY: rebuild
rebuild: distclean package


# ============================================================
# Inspection Targets
# ============================================================

# make list
#
# Print discovered source files, header files and include folders.
# Useful for checking whether Makefile discovery works correctly.
.PHONY: list
list:
	@echo.
	@echo ==================== C SOURCES ====================
	@echo $(C_SOURCES)
	@echo.
	@echo ==================== HEADERS ======================
	@echo $(H_SOURCES)
	@echo.
	@echo ==================== INCLUDE DIRS =================
	@echo $(INC_DIRS)


# make size
#
# Show size information for object files.
# Note:
#   This is mostly for inspection. The final .a library itself
#   is an archive, not a linked executable.
.PHONY: size
size: lib
	@echo.
	@echo [SIZE] Object file size summary
	$(SIZE) $(OBJECTS)


# make help
#
# Print available Makefile targets.
.PHONY: help
help:
	@echo.
	@echo EduFramework_v2 Makefile Targets
	@echo --------------------------------
	@echo make             Build package ^(same as make package^)
	@echo make package     Build library and export headers
	@echo make lib         Build libeduframework.a only
	@echo make headers     Export headers only
	@echo make clean       Remove build directory only
	@echo make distclean   Remove build and generated package files
	@echo make rebuild     Clean everything and rebuild package
	@echo make list        Show detected sources and include folders
	@echo make size        Show object file size summary
	@echo make help        Show this help message