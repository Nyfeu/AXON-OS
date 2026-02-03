#include "../../include/syscall.h"
#include "../../include/mutex.h"
#include "../../include/apps.h"
#include "../../include/apps_tasks.h"
#include "../../include/hal_uart.h" 
#include "../../include/hal_irq.h"  
#include "../../include/hal_plic.h" 
#include "../../include/circular_buffer.h"
#include "../../include/logger.h"

// ======================================================================================
// VARIÁVEIS GLOBAIS
// ======================================================================================

// Mutex para uso da UART
mutex_t uart_mutex;

// Buffer do teclado
cbuf_t  rx_buffer;

// ======================================================================================
// INICIALIZAÇÃO
// ======================================================================================

// Inicialização da aplicação 

void apps_init(void) {
    
    // 1. Inicializa mutexes e buffers
    mutex_init(&uart_mutex);
    cbuf_init(&rx_buffer);

    // 2. Configura a interrupção da UART no PLIC
    uint8_t plic_uart_id = get_uart_irq_id();

    // 3. Registra a ISR da UART
    hal_irq_register(plic_uart_id, uart_isr);
    
    // 4. Habilita a interrupção no PLIC
    hal_plic_set_priority(plic_uart_id, 1);
    hal_plic_enable(plic_uart_id);

}