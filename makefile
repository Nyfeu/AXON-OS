# ====================================================================
# CONFIGURAÇÕES DA TOOLCHAIN
# ====================================================================
CC = riscv32-unknown-elf-gcc
OBJCOPY = riscv32-unknown-elf-objcopy

# Flags Base (Comuns)
# -march=rv32i_zicsr: Arquitetura Base
# -Iinclude: Para achar timer.h e uart.h
CFLAGS_COMMON = -march=rv32i_zicsr -mabi=ilp32 -mcmodel=medany \
                -ffreestanding -O0 -g -Wall -Iinclude

# Diretórios de Saída
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj

# ====================================================================
# DEFINIÇÃO DE FONTES (SOURCES)
# ====================================================================

# 1. Fontes Comuns (Kernel e Bootloader)
# Estes arquivos são usados tanto no QEMU quanto na FPGA
SRCS_COMMON_C = $(wildcard src/kernel/*.c)
SRCS_COMMON_S = src/bsp/start.s src/kernel/trap.s

# 2. Fontes Específicos (Drivers)
# Usamos 'wildcard' para pegar tudo que está dentro das pastas novas
DRIVERS_QEMU = $(wildcard src/drivers/qemu/*.c)
DRIVERS_FPGA = $(wildcard src/drivers/fpga/*.c)

# 3. Listas Finais de Objetos
# A lógica abaixo cria caminhos espelhados em build/obj/qemu/src/...
# Nota: Colocamos SRCS_COMMON_S (Assembly) PRIMEIRO para garantir start.o no início

# --- Lista QEMU ---
OBJS_QEMU  = $(patsubst %.s, $(OBJ_DIR)/qemu/%.o, $(SRCS_COMMON_S))
OBJS_QEMU += $(patsubst %.c, $(OBJ_DIR)/qemu/%.o, $(SRCS_COMMON_C))
OBJS_QEMU += $(patsubst %.c, $(OBJ_DIR)/qemu/%.o, $(DRIVERS_QEMU))

# --- Lista FPGA ---
OBJS_FPGA  = $(patsubst %.s, $(OBJ_DIR)/fpga/%.o, $(SRCS_COMMON_S))
OBJS_FPGA += $(patsubst %.c, $(OBJ_DIR)/fpga/%.o, $(SRCS_COMMON_C))
OBJS_FPGA += $(patsubst %.c, $(OBJ_DIR)/fpga/%.o, $(DRIVERS_FPGA))

# ====================================================================
# TARGETS (ALVOS FINAIS)
# ====================================================================

TARGET_QEMU = $(BUILD_DIR)/kernel_qemu.elf
TARGET_FPGA = $(BUILD_DIR)/kernel_fpga.elf
BIN_FPGA    = $(BUILD_DIR)/kernel.bin

LINKER_QEMU = src/bsp/link.ld
LINKER_FPGA = src/bsp/link_fpga.ld

# Regra padrão: compila para QEMU
all: $(TARGET_QEMU)

# ====================================================================
# REGRAS DE COMPILAÇÃO: MODO QEMU
# ====================================================================

$(TARGET_QEMU): $(OBJS_QEMU) $(LINKER_QEMU)
	@echo "[QEMU] Linking Kernel..."
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -T $(LINKER_QEMU) -o $@ $(OBJS_QEMU) -nostdlib

# Compila C genérico para pasta QEMU
$(OBJ_DIR)/qemu/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -c $< -o $@

# Compila ASM genérico para pasta QEMU
$(OBJ_DIR)/qemu/%.o: %.s
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -c $< -o $@

# ====================================================================
# REGRAS DE COMPILAÇÃO: MODO FPGA
# ====================================================================

$(TARGET_FPGA): $(OBJS_FPGA) $(LINKER_FPGA)
	@echo "[FPGA] Linking Kernel..."
	@mkdir -p $(@D)
	# Adiciona flag -DPLATFORM_FPGA apenas aqui
	$(CC) $(CFLAGS_COMMON) -DPLATFORM_FPGA -T $(LINKER_FPGA) -o $@ $(OBJS_FPGA) -nostdlib

$(BIN_FPGA): $(TARGET_FPGA)
	@echo "[FPGA] Generating Binary..."
	$(OBJCOPY) -O binary $< $@

# Compila C genérico para pasta FPGA (Com define)
$(OBJ_DIR)/fpga/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -DPLATFORM_FPGA -c $< -o $@

# Compila ASM genérico para pasta FPGA (Com define)
$(OBJ_DIR)/fpga/%.o: %.s
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_COMMON) -DPLATFORM_FPGA -c $< -o $@

# ====================================================================
# COMANDOS UTILITÁRIOS
# ====================================================================

run: $(TARGET_QEMU)
	@echo "Starting QEMU..."
	qemu-system-riscv32 -nographic -machine virt -bios none -kernel $(TARGET_QEMU)

debug: $(TARGET_QEMU)
	qemu-system-riscv32 -nographic -machine virt -bios none -kernel $(TARGET_QEMU) -s -S

# Atalho para compilar e dizer que está pronto para upload
fpga: $(BIN_FPGA)
	@echo "Build complete for FPGA! Binary: $(BIN_FPGA)"

# Atalho para rodar seu script python (se tiver conectado)
upload: $(BIN_FPGA)
	@echo "Starting Upload..."
	python3 upload.py --file $(BIN_FPGA)

clean:
	rm -rf $(BUILD_DIR)