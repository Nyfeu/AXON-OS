# Toolchain
CC = riscv32-unknown-elf-gcc
OBJCOPY = riscv32-unknown-elf-objcopy

# Flags Base (Comuns)
CFLAGS_COMMON = -march=rv32i_zicsr -mabi=ilp32 -mcmodel=medany \
                -ffreestanding -O0 -g -Wall -Iinclude

# Diretórios
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj

# Fontes
SRCS_C = src/kernel/main.c src/drivers/uart.c src/drivers/timer.c
SRCS_S = src/bsp/start.s src/kernel/trap.s

# Objetos (QEMU vs FPGA são gerados em pastas diferentes para não misturar)
OBJS_QEMU = $(patsubst %.s, $(OBJ_DIR)/qemu/%.o, $(SRCS_S)) \
			$(patsubst %.c, $(OBJ_DIR)/qemu/%.o, $(SRCS_C)) 
            

OBJS_FPGA = $(patsubst %.s, $(OBJ_DIR)/fpga/%.o, $(SRCS_S)) \
			$(patsubst %.c, $(OBJ_DIR)/fpga/%.o, $(SRCS_C)) 
            

# Targets Finais
TARGET_QEMU = $(BUILD_DIR)/kernel_qemu.elf
TARGET_FPGA = $(BUILD_DIR)/kernel_fpga.elf
BIN_FPGA    = $(BUILD_DIR)/kernel.bin

LINKER_QEMU = src/bsp/link.ld
LINKER_FPGA = src/bsp/link_fpga.ld

# ====================================================================
# Regras Principais
# ====================================================================

all: $(TARGET_QEMU)

# --- MODO QEMU ---
$(TARGET_QEMU): $(OBJS_QEMU) $(LINKER_QEMU)
	@echo "Linking QEMU Kernel..."
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -T $(LINKER_QEMU) -o $@ $(OBJS_QEMU) -nostdlib

$(OBJ_DIR)/qemu/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -c $< -o $@

$(OBJ_DIR)/qemu/%.o: %.s
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -c $< -o $@

# --- MODO FPGA ---
# Adiciona a flag -DPLATFORM_FPGA na compilação
$(TARGET_FPGA): $(OBJS_FPGA) $(LINKER_FPGA)
	@echo "Linking FPGA Kernel..."
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -DPLATFORM_FPGA -T $(LINKER_FPGA) -o $@ $(OBJS_FPGA) -nostdlib

$(BIN_FPGA): $(TARGET_FPGA)
	@echo "Generating Binary for upload..."
	$(OBJCOPY) -O binary $< $@

$(OBJ_DIR)/fpga/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -DPLATFORM_FPGA -c $< -o $@

$(OBJ_DIR)/fpga/%.o: %.s
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -DPLATFORM_FPGA -c $< -o $@

# ====================================================================
# Comandos
# ====================================================================

# Compila e roda no QEMU
run: $(TARGET_QEMU)
	qemu-system-riscv32 -nographic -machine virt -bios none -kernel $(TARGET_QEMU)

# Compila apenas os artefatos da FPGA
fpga: $(BIN_FPGA)
	@echo "Build complete! File ready for upload: $(BIN_FPGA)"

debug: $(TARGET_QEMU)
	qemu-system-riscv32 -nographic -machine virt -bios none -kernel $(TARGET_QEMU) -s -S

clean:
	rm -rf $(BUILD_DIR)