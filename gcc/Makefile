# eZ430-Chronos V2.3 - MSP430-GCC 9.3.1.11
# Windows/MinGW GNU Make compatible. Target: CC430F6137.

PROJECT := ..
TARGET := ez430_chronos
FREQUENCY ?= 915
MCU := cc430f6137

# MSP430 GCC device support files (msp430.h + *.ld).
# Preferred: pass SUPPORT_DIR=C:/path/to/msp430-gcc/include
# The environment variable MSP430_GCC_INCLUDE_DIR is also accepted.
SUPPORT_DIR ?= $(MSP430_GCC_INCLUDE_DIR)

CC := msp430-elf-gcc
OBJCOPY := msp430-elf-objcopy
SIZE := msp430-elf-size

COMMON_CFLAGS := -mmcu=$(MCU) -msmall -mrelax -Os -flto -fsingle-precision-constant \
  -std=gnu99 -ffunction-sections -fdata-sections -fno-strict-aliasing \
  -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-missing-field-initializers

# Keep all floating constants at single precision. IAR MSP430 used 32-bit floating-point
# for this firmware; GCC otherwise promotes unsuffixed constants to 64-bit double and
# pulls several kilobytes of __mspabi_* double helpers into the 32-KB flash image.

# Radio/SimpliciTI support removed. FREQUENCY is kept only for compatible build paths.
COMMON_CPPFLAGS :=
ifeq ($(FREQUENCY),433)

else ifeq ($(FREQUENCY),868)

else ifeq ($(FREQUENCY),915)

else
$(error FREQUENCY must be 433, 868 or 915)
endif

INCLUDES := \
  -I$(PROJECT)/include \
  -I$(PROJECT)/driver \
  -I$(PROJECT)/logic

BUILD := build/$(FREQUENCY)

# GNU make on Windows does not provide the Unix 'find' command.  Keep the
# source list explicit so the build works from plain cmd.exe/PowerShell.
SRCS := $(wildcard \
  $(PROJECT)/driver/*.c \
  $(PROJECT)/logic/*.c)


# test.c is excluded in every original IAR Chronos configuration.
# The GCC no-radio startup no longer uses the old test_mode() entry point,
# so the placeholder stubs.c is not linked either.
SRCS := $(filter-out %/test.c %/stubs.c %/miscellaneous.c,$(SRCS))
# Complete RF stack removed: no CC1101 driver, RF1A ISR, SimpliciTI UI, or RFBSL.
SRCS := $(filter-out %/radio.c %/rf1a.c %/rfsimpliciti.c %/rfbsl.c,$(SRCS))
SRCS += $(PROJECT)/main.c

OBJS := $(patsubst $(PROJECT)/%.c,$(BUILD)/%.o,$(SRCS))

ifneq ($(strip $(SUPPORT_DIR)),)
SUPPORT_FLAGS := -I"$(SUPPORT_DIR)" -L"$(SUPPORT_DIR)"
else
SUPPORT_FLAGS :=
endif

CFLAGS := $(COMMON_CFLAGS) $(SUPPORT_FLAGS) $(INCLUDES)
CPPFLAGS := $(COMMON_CPPFLAGS)
LDFLAGS := $(if $(strip $(SUPPORT_DIR)),-L"$(SUPPORT_DIR)",) -flto -mrelax -Wl,--gc-sections -Wl,-Map,$(BUILD)/$(TARGET).map

.PHONY: all clean size info

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin size

# Windows cmd.exe recipe: create the output directory if it does not exist.
$(BUILD)/%.o: $(PROJECT)/%.c
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

$(BUILD)/$(TARGET).elf: $(OBJS)
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o "$@"

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex "$<" "$@"

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary "$<" "$@"

size: $(BUILD)/$(TARGET).elf
	$(SIZE) "$<"

info:
	@echo MCU=$(MCU)
	@echo FREQUENCY=$(FREQUENCY)
	@echo CC=$(CC)
	@echo SUPPORT_DIR=$(SUPPORT_DIR)
	@echo Sources=$(words $(SRCS))

clean:
	@if exist build rmdir /s /q build
