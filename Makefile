# ============================================================
# EduFramework v2.0.0 Makefile
# Target  : NXP S32K144 / MaaZEDU
# Purpose : Build EduFramework static library package
#
# Generated package:
#   eduframework/include/*.h
#   eduframework/lib/libeduframework.a
#
# Source structure supported:
#
#   firmware/
#   ├── arduino/
#   │   ├── inc/
#   │   └── src/
#   ├── board/
#   │   └── inc/
#   ├── core/
#   │   └── inc/
#   ├── drivers/
#   │   └── <driver>/
#   │       ├── inc/
#   │       └── src/
#   └── devices/
#       └── <device>/
#           ├── device.c
#           └── device.h
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
#
# Override example:
#
#   make TOOLCHAIN_DIR=C:/path/to/toolchain/bin
#
TOOLCHAIN_DIR ?= C:/NXP/S32DS.3.5/S32DS/build_tools/gcc_v10.2/gcc-10.2-arm32-eabi/bin

# C compiler.
CC := $(TOOLCHAIN_DIR)/arm-none-eabi-gcc.exe

# Static library archiver.
AR := $(TOOLCHAIN_DIR)/arm-none-eabi-ar.exe

# Size utility.
SIZE := $(TOOLCHAIN_DIR)/arm-none-eabi-size.exe


# ============================================================
# Project Directories
# ============================================================

# Framework source root.
FIRMWARE_DIR := firmware

# S32K144 CMSIS/device headers supplied by the S32DS project.
S32DS_INCLUDE_DIR := s32ds/EduFramework/include

# Temporary build directory.
BUILD_DIR := build

# Object file directory.
OBJ_DIR := $(BUILD_DIR)/obj

# Exported framework package.
PACKAGE_DIR := eduframework

# Exported public headers.
PACKAGE_INC_DIR := $(PACKAGE_DIR)/include

# Exported static library.
PACKAGE_LIB_DIR := $(PACKAGE_DIR)/lib

# Final library file.
TARGET_LIB := $(PACKAGE_LIB_DIR)/libeduframework.a


# ============================================================
# Automatic Source Discovery
# ============================================================

# ------------------------------------------------------------
# C Sources
# ------------------------------------------------------------
#
# Discover every .c file recursively under firmware/.
#
# Examples:
#
#   firmware/arduino/src/Arduino.c
#   firmware/drivers/gpio/src/gpio.c
#   firmware/devices/NTC/ntc.c
#   firmware/devices/RFID/rc522.c
#
# Every discovered source is compiled into libeduframework.a.
#
C_SOURCES := $(shell powershell -NoProfile -Command \
	"Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -File -Filter *.c | \
	ForEach-Object { \
		$$_.FullName.Replace((Get-Location).Path + '\','').Replace('\','/') \
	}")


# ------------------------------------------------------------
# Header Sources
# ------------------------------------------------------------
#
# Discover every .h file recursively under firmware/.
#
# Every discovered header is exported into:
#
#   eduframework/include/
#
H_SOURCES := $(shell powershell -NoProfile -Command \
	"Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -File -Filter *.h | \
	ForEach-Object { \
		$$_.FullName.Replace((Get-Location).Path + '\','').Replace('\','/') \
	}")


# ------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------
#
# Discover every directory that contains at least one .h file.
#
# This intentionally supports BOTH framework layouts:
#
# Traditional driver layout:
#
#   firmware/drivers/gpio/inc/gpio.h
#
# Device layout:
#
#   firmware/devices/NTC/ntc.h
#
# Duplicate directories are removed automatically.
#
INC_DIRS := $(shell powershell -NoProfile -Command \
	"Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -File -Filter *.h | \
	ForEach-Object { \
		$$_.DirectoryName.Replace((Get-Location).Path + '\','').Replace('\','/') \
	} | Sort-Object -Unique")


# ------------------------------------------------------------
# Object Files
# ------------------------------------------------------------
#
# Preserve the original firmware directory structure inside
# build/obj/ to prevent object filename collisions.
#
# Example:
#
#   firmware/devices/NTC/ntc.c
#
# becomes:
#
#   build/obj/firmware/devices/NTC/ntc.o
#
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SOURCES))


# ------------------------------------------------------------
# Compiler Include Flags
# ------------------------------------------------------------

# Add all automatically discovered framework header directories.
INCLUDES := $(addprefix -I,$(INC_DIRS))

# Add S32K144 device/CMSIS header directory.
INCLUDES += -I$(S32DS_INCLUDE_DIR)


# ============================================================
# Compiler Configuration
# ============================================================

# Cortex-M4F configuration for S32K144.
CPU_FLAGS := \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard


# Device macro expected by the NXP S32K144 headers.
DEFINES := \
	-DCPU_S32K144HFT0VLLT


# Common compiler flags.
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
# Equivalent to:
#
#   make package
#
.PHONY: all
all: package


# ============================================================
# Package Target
# ============================================================

# make package
#
# Generates:
#
#   eduframework/include/*.h
#   eduframework/lib/libeduframework.a
#
.PHONY: package
package: lib headers
	@echo.
	@echo ============================================================
	@echo [DONE] EduFramework package generated
	@echo ============================================================
	@echo [LIB ] $(TARGET_LIB)
	@echo [INC ] $(PACKAGE_INC_DIR)
	@echo.


# ============================================================
# Library Target
# ============================================================

# make lib
#
# Compile every firmware/*.c source and archive all resulting
# objects into libeduframework.a.
#
.PHONY: lib
lib: $(TARGET_LIB)


# Build final static library.
$(TARGET_LIB): $(OBJECTS)
	@echo.
	@echo ============================================================
	@echo [AR  ] Creating static library
	@echo ============================================================
	@echo [OUT ] $@
	@echo.

	@if not exist "$(subst /,\,$(PACKAGE_LIB_DIR))" mkdir "$(subst /,\,$(PACKAGE_LIB_DIR))"

	$(AR) $(ARFLAGS) $@ $(OBJECTS)


# ============================================================
# Compile Rule
# ============================================================

# Compile each C source into its corresponding object file.
#
# Source:
#
#   firmware/x/y/file.c
#
# Output:
#
#   build/obj/firmware/x/y/file.o
#
$(OBJ_DIR)/%.o: %.c
	@echo [CC  ] $<

	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"

	$(CC) $(CFLAGS) -c $< -o $@


# ============================================================
# Header Export Target
# ============================================================

# make headers
#
# Export every firmware/*.h header into a flat public include
# directory:
#
#   eduframework/include/
#
# Current framework headers have unique filenames.
#
# IMPORTANT:
# If duplicate public header filenames are introduced in the
# future, this export strategy must be changed to preserve the
# source directory hierarchy.
#
.PHONY: headers
headers:
	@echo.
	@echo ============================================================
	@echo [HDR ] Exporting framework headers
	@echo ============================================================

	@if not exist "$(subst /,\,$(PACKAGE_INC_DIR))" mkdir "$(subst /,\,$(PACKAGE_INC_DIR))"

	powershell -NoProfile -Command \
		"Get-ChildItem '$(FIRMWARE_DIR)' -Recurse -File -Filter *.h | \
		Copy-Item -Destination '$(PACKAGE_INC_DIR)' -Force"

	@echo [HDR ] Header export complete
	@echo.


# ============================================================
# Clean Targets
# ============================================================

# make clean
#
# Remove temporary compilation artifacts only.
#
# The generated package remains untouched:
#
#   eduframework/include/
#   eduframework/lib/
#
.PHONY: clean
clean:
	@echo.
	@echo [CLEAN] Removing $(BUILD_DIR)

	@if exist "$(BUILD_DIR)" rmdir /S /Q "$(BUILD_DIR)"

	@echo [CLEAN] Done
	@echo.


# ------------------------------------------------------------
# make distclean
# ------------------------------------------------------------
#
# Remove:
#
#   build/
#   eduframework/lib/libeduframework.a
#   eduframework/include/*.h
#
# Use this before creating an official release package.
#
.PHONY: distclean
distclean: clean
	@echo [CLEAN] Removing generated framework package

	@if exist "$(subst /,\,$(TARGET_LIB))" del /Q "$(subst /,\,$(TARGET_LIB))"

	@if exist "$(subst /,\,$(PACKAGE_INC_DIR))\*.h" del /Q "$(subst /,\,$(PACKAGE_INC_DIR))\*.h"

	@echo [CLEAN] Package removed
	@echo.


# ------------------------------------------------------------
# make rebuild
# ------------------------------------------------------------
#
# Fully regenerate the framework package from firmware/.
#
.PHONY: rebuild
rebuild: distclean package


# ============================================================
# Inspection Targets
# ============================================================

# make list
#
# Display automatically discovered:
#
#   - C sources
#   - Header sources
#   - Include directories
#
# Useful when adding new drivers or device modules.
#
.PHONY: list
list:
	@echo.
	@echo ============================================================
	@echo C SOURCES
	@echo ============================================================
	@powershell -NoProfile -Command "$$items='$(C_SOURCES)'.Split(' '); $$items | ForEach-Object { Write-Host $$_.Trim() }"

	@echo.
	@echo ============================================================
	@echo HEADERS
	@echo ============================================================
	@powershell -NoProfile -Command "$$items='$(H_SOURCES)'.Split(' '); $$items | ForEach-Object { Write-Host $$_.Trim() }"

	@echo.
	@echo ============================================================
	@echo INCLUDE DIRECTORIES
	@echo ============================================================
	@powershell -NoProfile -Command "$$items='$(INC_DIRS)'.Split(' '); $$items | ForEach-Object { Write-Host $$_.Trim() }"

	@echo.


# ============================================================
# Library Content Inspection
# ============================================================

# make archive-list
#
# Show every object stored inside libeduframework.a.
#
# Useful before publishing a release to verify that device
# modules are actually included in the library.
#
.PHONY: archive-list
archive-list: lib
	@echo.
	@echo ============================================================
	@echo LIBRARY OBJECTS
	@echo ============================================================
	$(AR) t $(TARGET_LIB)
	@echo.


# ============================================================
# Size Inspection
# ============================================================

# make size
#
# Display size information for all generated object files.
#
# Note:
# libeduframework.a is an archive. Final flash/RAM usage depends
# on which symbols are referenced by the final application.
#
.PHONY: size
size: lib
	@echo.
	@echo ============================================================
	@echo OBJECT FILE SIZE SUMMARY
	@echo ============================================================
	$(SIZE) $(OBJECTS)
	@echo.


# ============================================================
# Verification Target
# ============================================================

# make verify
#
# Basic release-package verification:
#
#   1. Build package.
#   2. Confirm libeduframework.a exists.
#   3. Confirm exported include directory exists.
#
.PHONY: verify
verify: package
	@echo.
	@echo ============================================================
	@echo VERIFYING PACKAGE
	@echo ============================================================

	@if not exist "$(subst /,\,$(TARGET_LIB))" ( \
		echo [ERROR] Missing $(TARGET_LIB) & \
		exit /B 1 \
	)

	@if not exist "$(subst /,\,$(PACKAGE_INC_DIR))" ( \
		echo [ERROR] Missing $(PACKAGE_INC_DIR) & \
		exit /B 1 \
	)

	@echo [PASS] Static library exists
	@echo [PASS] Public include directory exists
	@echo [PASS] EduFramework package verification complete
	@echo.


# ============================================================
# Help
# ============================================================

.PHONY: help
help:
	@echo.
	@echo ============================================================
	@echo EduFramework v2.0.0 Makefile
	@echo ============================================================
	@echo.
	@echo make
	@echo     Build complete framework package.
	@echo.
	@echo make package
	@echo     Build libeduframework.a and export public headers.
	@echo.
	@echo make lib
	@echo     Build libeduframework.a only.
	@echo.
	@echo make headers
	@echo     Export all firmware headers.
	@echo.
	@echo make clean
	@echo     Remove temporary build directory.
	@echo.
	@echo make distclean
	@echo     Remove build artifacts and generated package.
	@echo.
	@echo make rebuild
	@echo     Completely regenerate the framework package.
	@echo.
	@echo make list
	@echo     Show discovered sources, headers and include paths.
	@echo.
	@echo make archive-list
	@echo     Show object files stored inside libeduframework.a.
	@echo.
	@echo make size
	@echo     Show object-file size information.
	@echo.
	@echo make verify
	@echo     Build and verify generated package.
	@echo.
	@echo make help
	@echo     Show this help message.
	@echo.