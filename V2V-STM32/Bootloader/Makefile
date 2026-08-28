##
## Makefile — FOTA bootloader for the STM32F446RE V2V safety core.
##
## Deliberately a plain GNU Makefile, NOT a second STM32CubeIDE project: the
## main V2V-STM32 firmware stays exactly as it is (still built/debugged from
## CubeIDE, unchanged), and this small, separate binary is easy to build,
## inspect and rebuild from a terminal or CI with no IDE project metadata to
## keep in sync. See ../docs/FOTA.md and README.md (in this folder) for the
## full design and how to build/flash/test this.
##
## It reuses several MCAL drivers straight from the main firmware's own
## Src/Inc tree (RCC, GPIO, SCB, TIM, USART, FLASH) plus the FOTA metadata/
## CRC32 modules, rather than duplicating a second copy of any of them — the
## bootloader and the application share exactly one implementation of each.
##
## Requires the GNU Arm Embedded / "GNU Tools for STM32" toolchain
## (arm-none-eabi-gcc) on PATH — the same one STM32CubeIDE bundles. See
## README.md for exactly where to find it if you only have CubeIDE installed.
##

TARGET      = bootloader
BUILD_DIR   = build

PROJECT_ROOT = ..

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

# This bootloader's own code.
C_SOURCES = \
  Src/Bootloader_main.c

# Startup file — reused UNCHANGED from the main firmware. It only depends on
# linker-provided symbols (_estack, _sdata, ...), so it works as-is against
# Bootloader_FLASH.ld exactly like it does against the application's own
# linker script.
ASM_SOURCES = \
  $(PROJECT_ROOT)/Startup/startup_stm32f446retx.s

# MCAL drivers + FOTA application modules, reused directly from the main
# firmware's Src/ folder — NOT copied. One implementation of each, shared by
# both the bootloader and the application.
C_SOURCES += \
  $(PROJECT_ROOT)/Src/RCC_program.c \
  $(PROJECT_ROOT)/Src/GPIO_prog.c \
  $(PROJECT_ROOT)/Src/SCB_program.c \
  $(PROJECT_ROOT)/Src/TIM_program.c \
  $(PROJECT_ROOT)/Src/USART_program.c \
  $(PROJECT_ROOT)/Src/FLASH_program.c \
  $(PROJECT_ROOT)/Src/FOTA_CRC32_program.c \
  $(PROJECT_ROOT)/Src/FOTA_Metadata_program.c

# ---------------------------------------------------------------------------
# MCU / build flags — mirrors what STM32CubeIDE generates for this exact
# target (NUCLEO-F446RE / STM32F446RETx, Cortex-M4F with hardware FPU, the
# same ABI the FreeRTOS ARM_CM4F port in the main firmware assumes).
# ---------------------------------------------------------------------------

MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

C_DEFS =

# Only -I the bootloader's own Inc, if it ever gets one. Every reused driver
# source includes its own headers via ITS OWN relative paths (e.g.
# "../Inc/Drivers/MCAL/RCC/RCC_interface.h" inside RCC_program.c, which is
# still physically at V2V-STM32/Src/RCC_program.c and resolves correctly no
# matter where `make` is invoked from), so no extra -I is needed to make the
# shared drivers see their own headers.
C_INCLUDES =

CFLAGS = $(MCU_FLAGS) $(C_DEFS) $(C_INCLUDES) \
         -Wall -Wextra -std=c11 -O2 -g3 \
         -ffunction-sections -fdata-sections \
         -MMD -MP

ASFLAGS = $(MCU_FLAGS) -x assembler-with-cpp -MMD -MP

# -specs=nosys.specs / nano.specs: this bootloader calls no stdio and uses no
# heap, but the shared startup file's Reset_Handler unconditionally calls
# __libc_init_array, and the toolchain otherwise expects a hosted libc
# underneath it. These specs supply stub syscalls so the link succeeds
# without needing to build ../Src/syscalls.c in as well.
LDFLAGS = $(MCU_FLAGS) -specs=nosys.specs -specs=nano.specs \
          -T Bootloader_FLASH.ld \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
          -Wl,--gc-sections

# ---------------------------------------------------------------------------
# Build rules
# ---------------------------------------------------------------------------

OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

# vpath tells make where to actually find each source, since OBJECTS above
# only keeps the basename (two different source dirs could otherwise collide
# on a filename — they do not today, but this keeps the object dir flat and
# simple regardless).
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(CC) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Bootloader_FLASH.ld | $(BUILD_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

# ---------------------------------------------------------------------------
# Flashing (ST-Link). Requires STM32_Programmer_CLI (comes with
# STM32CubeIDE/STM32CubeProgrammer) on PATH.
# ---------------------------------------------------------------------------

flash: $(BUILD_DIR)/$(TARGET).bin
	STM32_Programmer_CLI -c port=SWD -d $(BUILD_DIR)/$(TARGET).bin 0x08000000 -v -rst

.PHONY: all clean flash

-include $(OBJECTS:.o=.d)
