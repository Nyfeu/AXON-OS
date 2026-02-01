#include "../../../include/hal_timer.h"
#include "memory_map.h" // Acesso aos registradores do QEMU

#define SYSTEM_CLOCK_HZ 10000000  // 10 MHz (Clock base do QEMU 'virt')

// ============================================================================
// FUNÇÕES AUXILIARES (Internas)
// ============================================================================

// Helper para escrever no comparador de 64 bits de forma segura em sistema 32 bits
static void hal_clint_set_cmp(uint64_t cycles) {
    // 1. Define High como "infinito" (tudo 1s) para evitar disparo prematuro
    CLINT_MTIMECMP_HI = 0xFFFFFFFF;
    
    // 2. Escreve a parte baixa
    CLINT_MTIMECMP_LO = (uint32_t)(cycles & 0xFFFFFFFF);
    
    // 3. Escreve a parte alta correta
    CLINT_MTIMECMP_HI = (uint32_t)(cycles >> 32);
}

// ============================================================================
// IMPLEMENTAÇÃO DA API
// ============================================================================

void hal_timer_reset(void) {
    hal_clint_set_cmp(0xFFFFFFFFFFFFFFFF); // Desarma
    CLINT_MTIME_LO = 0;
    CLINT_MTIME_HI = 0;
}

uint64_t hal_timer_get_cycles(void) {
    volatile uint32_t hi, lo, hi2;
    do {
        hi  = CLINT_MTIME_HI;
        lo  = CLINT_MTIME_LO;
        hi2 = CLINT_MTIME_HI;
    } while (hi != hi2); // Garante que não houve overflow durante a leitura
    
    return ((uint64_t)hi << 32) | lo;
}

void hal_timer_set_irq_delta(uint64_t delta_cycles) {
    uint64_t now = hal_timer_get_cycles();
    hal_clint_set_cmp(now + delta_cycles);
}

void hal_timer_irq_ack(void) {
    hal_clint_set_cmp(0xFFFFFFFFFFFFFFFF);
}

// ============================================================================
// IMPLEMENTAÇÃO DE DELAYS
// ============================================================================

void hal_timer_delay_us(uint32_t us) {
    uint64_t start = hal_timer_get_cycles();
    // 1 us = 10 ciclos (@ 10MHz)
    uint64_t cycles_to_wait = (uint64_t)us * 10;
    while ((hal_timer_get_cycles() - start) < cycles_to_wait);
}

void hal_timer_delay_ms(uint32_t ms) {
    uint64_t start = hal_timer_get_cycles();
    // 1 ms = 10.000 ciclos (@ 10MHz)
    uint64_t cycles_to_wait = (uint64_t)ms * 10000;
    while ((hal_timer_get_cycles() - start) < cycles_to_wait);
}