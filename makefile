# Toolchain
CC = riscv32-unknown-elf-gcc
OBJCOPY = riscv32-unknown-elf-objcopy

# Flags
CFLAGS = -march=rv32i_zicsr -mabi=ilp32 -mcmodel=medany \
         -ffreestanding -O0 -g -Wall -Iinclude

# Diretório de build
BUILD_DIR = build

# Fontes
SRCS_C = src/kernel/main.c src/drivers/uart.c src/drivers/timer.c
SRCS_S = src/bsp/start.s src/kernel/trap.s
SRCS = $(SRCS_S) $(SRCS_C)

# Converte src/... -> build/src/....o
OBJS = $(patsubst %.s,$(BUILD_DIR)/%.o,$(SRCS_S)) \
       $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS_C))

TARGET = $(BUILD_DIR)/kernel.elf

all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -T src/bsp/link.ld -o $@ $^ -nostdlib

# Regra genérica para C
$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Regra genérica para Assembly
$(BUILD_DIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# QEMU
run: $(TARGET)
	qemu-system-riscv32 -nographic -machine virt -bios none -kernel $(TARGET)

debug: $(TARGET)
	qemu-system-riscv32 -nographic -machine virt -bios none -kernel $(TARGET) -s -S

# Clean total
clean:
	rm -rf $(BUILD_DIR)
	find src -name '*.o' -delete
