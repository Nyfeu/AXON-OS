#include "../../include/uart.h"
#include "../bsp/memory_map.h"

void uart_init() {
    // No QEMU virt, a UART 16550 já vem pré-inicializada pelo firmware interno
    // em 115200 8N1. Não precisamos configurar baudrate agora.
    
    // Habilita FIFO (opcional, mas boa prática no 16550)
    MMIO8(UART0_BASE + 0x02) = 0x01; 
}

void uart_putc(char c) {
    // Espera o buffer de transmissão estar vazio (Bit 5 do Line Status Register)
    // Offset 0x05 é o LSR (Line Status Register)
    while ((MMIO8(UART0_BASE + 0x05) & 0x20) == 0);
    
    // Escreve o caractere no registrador de dados (Offset 0x00)
    MMIO8(UART0_BASE + 0x00) = c;
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}