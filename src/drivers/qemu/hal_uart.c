#include "../../../include/hal/hal_uart.h"
#include "memory_map.h"

#define UART_IRQ_ID 10

void hal_uart_init(void) {
    // 1. Desabilita interrupções temporariamente
    MMIO8(UART0_BASE + UART_IER) = 0x00;
    
    // 2. Configura Baud Rate (Opcional no QEMU, mas boa prática)
    // Habilita acesso ao Divisor Latch (DLAB=1)
    MMIO8(UART0_BASE + UART_LCR) = 0x80; 
    MMIO8(UART0_BASE + 0x00) = 0x03; // Divisor Low (38.4K simulado)
    MMIO8(UART0_BASE + 0x01) = 0x00; // Divisor High
    
    // 3. Configura 8 bits, Sem Paridade, 1 Stop Bit (8N1) e limpa DLAB
    MMIO8(UART0_BASE + UART_LCR) = 0x03;
    
    // 4. Habilita FIFO
    MMIO8(UART0_BASE + UART_FCR) = 0x01;
    
    // 5. Reabilita interrupções (Received Data Available)
    MMIO8(UART0_BASE + UART_IER) = 0x01;
}

void hal_uart_putc(char c) {
    // Espera o buffer de transmissão ficar vazio (LSR Bit 5)
    while ((MMIO8(UART0_BASE + UART_LSR) & 0x20) == 0);
    
    // Escreve o caractere
    MMIO8(UART0_BASE + UART_THR) = c;
}

void hal_uart_puts(const char* str) {
    while (*str) hal_uart_putc(*str++);
}

// Funções de Leitura 
int hal_uart_kbhit(void) {
    // LSR Bit 0 = Data Ready
    return (MMIO8(UART0_BASE + UART_LSR) & 0x01);
}

char hal_uart_getc(void) {
    while (!hal_uart_kbhit()); // Bloqueante
    return (char)MMIO8(UART0_BASE + UART_RBR);
}

uint8_t get_uart_irq_id() {
    return UART_IRQ_ID;
}