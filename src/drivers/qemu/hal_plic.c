#include "../../../include/hal_plic.h"
#include "memory_map.h"

// ============================================================================
// IMPLEMENTAÇÃO (Cópia adaptada para QEMU via memory_map.h)
// ============================================================================

void hal_plic_init(void) {
    // 1. Desabilitar todas as interrupções (Enable Register = 0)
    PLIC_ENABLE = 0x00000000;

    // 2. Zerar Threshold (Permitir qualquer prioridade > 0)
    PLIC_THRESHOLD = 0;

    // 3. Limpar prioridades (Fontes 1..31)
    // No QEMU 'virt', UART0 é geralmente a IRQ 10
    for (int i = 1; i <= 31; i++) {
        PLIC_PRIORITY(i) = 0;
    }
    
    // Configura prioridade da UART (IRQ 10) para 1 (habilitada)
    // Isso garante que a UART possa interromper
    PLIC_PRIORITY(10) = 1; 
}

void hal_plic_enable(uint32_t source_id) {
    // Read-Modify-Write no registrador de Enable
    uint32_t current_enables = PLIC_ENABLE;
    current_enables |= (1 << source_id);
    PLIC_ENABLE = current_enables;
}

void hal_plic_disable(uint32_t source_id) {
    uint32_t current_enables = PLIC_ENABLE;
    current_enables &= ~(1 << source_id);
    PLIC_ENABLE = current_enables;
}

void hal_plic_set_priority(uint32_t source_id, uint32_t priority) {
    PLIC_PRIORITY(source_id) = priority;
}

void hal_plic_set_threshold(uint32_t threshold) {
    PLIC_THRESHOLD = threshold;
}

uint32_t hal_plic_claim(void) {
    return PLIC_CLAIM;
}

void hal_plic_complete(uint32_t source_id) {
    PLIC_CLAIM = source_id;
}