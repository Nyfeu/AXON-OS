#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

// ============================================================================
// API DO TIMER (Protótipos)
// ============================================================================

/**
 * @brief Retorna a frequência base do Timer em Hz.
 * Útil para calcular delays e timeouts independente da plataforma.
 */
uint32_t hal_timer_get_freq(void);

/**
 * @brief Entra em estado IDLE.
 * Utiliza WFI para economizar energia ou NOP para não realizar nenhuma tarefa.
 */
void hal_timer_idle(void);

/**
 * @brief Reinicia o contador de tempo (Escreve 0 no mtime).
 */
void hal_timer_reset(void);

/**
 * @brief Captura o tempo atual de forma atômica.
 * @return Ciclos contados (64-bit).
 */
uint64_t hal_timer_get_cycles(void);

/**
 * @brief Configura o timer para gerar uma interrupção daqui a N ciclos.
 * @param delta_cycles Quantidade de ciclos a esperar.
 */
void hal_timer_set_irq_delta(uint64_t delta_cycles);

/**
 * @brief Desativa (ack) a interrupção do timer.
 */
void hal_timer_irq_ack(void);

// ============================================================================
// FUNÇÕES DE DELAY
// ============================================================================

void hal_timer_delay_us(uint32_t us);
void hal_timer_delay_ms(uint32_t ms);

#endif /* HAL_TIMER_H */