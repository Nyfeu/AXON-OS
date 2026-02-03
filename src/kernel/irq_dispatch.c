#include "../include/hal/hal_irq.h"
#include "../include/hal/hal_plic.h"
#include <stddef.h>

// Tabela de Vetores de Interrupção (RAM)
// Mapeia ID do PLIC -> Função C void func(void)
static irq_handler_t g_isr_table[PLIC_MAX_SOURCES] = { NULL };

// ----------------------------------------------------------------------------
// IMPLEMENTAÇÃO DA API 
// ----------------------------------------------------------------------------

void hal_irq_init(void) {

    // 1. Inicializa o controlador PLIC (zera prioridades e enables)
    hal_plic_init();

    // 2. Limpa a tabela de software
    for (int i = 0; i < PLIC_MAX_SOURCES; i++) g_isr_table[i] = NULL;

    // 3. Habilita Interrupções Externas no nível do processador (Bit 11 mie)
    hal_irq_mask_enable(IRQ_M_EXT);

}

void hal_irq_register(uint32_t source_id, irq_handler_t handler) {

    if (source_id < PLIC_MAX_SOURCES) {

        g_isr_table[source_id] = handler;
        
        // Já habilitar no PLIC automaticamente ao registrar
        hal_plic_set_priority(source_id, 1);
        hal_plic_enable(source_id);
    
    }
    
}

// ----------------------------------------------------------------------------
// DESPACHANTE (Chamado pelo trap_handler em main.c)
// ----------------------------------------------------------------------------

void irq_dispatch(uint32_t source) {

    // Verifica se o ID é válido e se existe função registrada
    if (source > 0 && source < PLIC_MAX_SOURCES) {
        if (g_isr_table[source] != NULL) {
            g_isr_table[source](); // Executa o Callback da Aplicação
        }
    }

}