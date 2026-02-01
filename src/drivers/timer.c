#include "../../include/uart.h"
#include "../bsp/memory_map.h"

// Intervalo do Tick (aprox. 1 segundo no QEMU base clock de 10MHz)
// Se ficar muito lento, diminua para 1000000
#define TICK_INTERVAL 10000000 

// Protótipo da função (definida no timer.h depois)
void timer_init();
void timer_handler();

void timer_init() {
    // 1. Ler o tempo atual (mtime)
    unsigned long long current_time = MMIO64(CLINT_MTIME);

    // 2. Configurar a próxima interrupção (mtimecmp = atual + intervalo)
    MMIO64(CLINT_MTIMECMP(0)) = current_time + TICK_INTERVAL;

    // 3. Habilitar a interrupção de Timer no bit MTIE do registrador mie
    // mie (Machine Interrupt Enable) -> bit 7 é MTIE
    unsigned int mie;
    asm volatile("csrr %0, mie" : "=r"(mie));
    mie |= (1 << 7);
    asm volatile("csrw mie, %0" : : "r"(mie));
}

// Chamado pelo trap_handler quando for uma interrupção de timer
void timer_handler() {
    uart_puts("Tick! \n\r");

    // Re-programar o próximo tick
    unsigned long long current_time = MMIO64(CLINT_MTIME);
    MMIO64(CLINT_MTIMECMP(0)) = current_time + TICK_INTERVAL;
}