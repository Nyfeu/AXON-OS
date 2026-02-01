#include "hal_timer.h"
#include "math_ops.h"
#include "memory_map.h"

// ============================================================================
// FUNÇÕES DE DELAY (BLOQUEANTE)
// ============================================================================

void hal_timer_delay_us(uint32_t us) {

    uint64_t start = hal_timer_get_cycles();
    
    // Otimização Bare-Metal:
    // 1 us = 100 ciclos (@ 100MHz).
    // Nota: Cuidado com overflow se us for muito grande (uint32_t max * 100).
    uint64_t cycles_to_wait = (uint64_t)us * (SYSTEM_CLOCK_HZ / 1000000);
    
    while ((hal_timer_get_cycles() - start) < cycles_to_wait);

}

void hal_timer_delay_ms(uint32_t ms) {

    uint64_t start = hal_timer_get_cycles();
    
    // 1 ms = 100.000 ciclos (@ 100MHz).
    uint64_t cycles_to_wait = (uint64_t)ms * (SYSTEM_CLOCK_HZ / 1000);
    
    while ((hal_timer_get_cycles() - start) < cycles_to_wait);
    
}

// ============================================================================
// API  
// ============================================================================

void hal_timer_reset(void) {
    // Para segurança, escrevemos -1 no CMP antes para evitar IRQ espúria durante o reset
    CLINT_MTIMECMP_LO = 0xFFFFFFFF;
    CLINT_MTIMECMP_HI = 0xFFFFFFFF;
    
    CLINT_MTIME_LO = 0;
    CLINT_MTIME_HI = 0;
}

uint64_t hal_timer_get_cycles(void) {
    uint32_t hi, lo, hi2;

    do {
        hi  = CLINT_MTIME_HI;
        lo  = CLINT_MTIME_LO;
        hi2 = CLINT_MTIME_HI;
    } while (hi != hi2); // Repete se houve overflow do Low para o High durante a leitura
    
    return ((uint64_t)hi << 32) | lo;
}

void hal_clint_set_cmp(uint64_t cycles) {
    // Programamos o High como MAX primeiro para evitar disparo acidental
    CLINT_MTIMECMP_HI = 0xFFFFFFFF; 
    CLINT_MTIMECMP_LO = (uint32_t)(cycles & 0xFFFFFFFF);
    CLINT_MTIMECMP_HI = (uint32_t)(cycles >> 32);
}

void hal_timer_set_irq_delta(uint64_t delta_cycles) {
    uint64_t now = hal_timer_get_cycles();
    hal_clint_set_cmp(now + delta_cycles);
}

void hal_timer_irq_ack(void) {
    hal_clint_set_cmp(0xFFFFFFFFFFFFFFFF);
}

uint32_t hal_timer_get_freq(void) {
    return SYSTEM_CLOCK_HZ;
}

void hal_timer_idle(void) {
    asm volatile("nop"); 
}