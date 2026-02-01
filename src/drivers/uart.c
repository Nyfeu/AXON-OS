#include "../../include/uart.h"
#include "../bsp/memory_map.h"

// Definições Específicas da FPGA 
#ifdef PLATFORM_FPGA
    #define UART_STATUS_TX_BUSY  (1 << 0)
    #define UART_CMD_RX_FLUSH    (1 << 2)
#endif

void uart_init() {
#ifdef PLATFORM_FPGA
    // [FPGA] Flush na FIFO para limpar lixo de boot
    MMIO32(UART_REG_CTRL) = UART_CMD_RX_FLUSH;
#else
    // [QEMU] Inicialização opcional da FIFO 16550
    MMIO8(UART0_BASE + 0x02) = 0x01; 
#endif
}

void uart_putc(char c) {
#ifdef PLATFORM_FPGA
    // [FPGA] Lógica Customizada
    // Espera bit 0 (TX_BUSY) baixar
    while (MMIO32(UART_REG_CTRL) & UART_STATUS_TX_BUSY);
    // Escreve no registrador de dados (Offset 0x00)
    MMIO32(UART_REG_DATA) = c;
#else
    // [QEMU] Lógica 16550 Padrão
    // Espera bit 5 (Transmitter Holding Register Empty) do LSR (Offset 5)
    while ((MMIO8(UART0_BASE + 0x05) & 0x20) == 0);
    MMIO8(UART0_BASE + 0x00) = c;
#endif
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}