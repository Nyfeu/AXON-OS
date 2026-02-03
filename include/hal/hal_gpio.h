#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

/**
 * @brief Inicializa o driver de GPIO (se necessário)
 */
void hal_gpio_init(void);

/**
 * @brief Escreve no registrador de LEDs
 * @param val Valor de 16 bits para exibir nos LEDs
 */
void hal_gpio_write(uint16_t val);

/**
 * @brief Lê o estado atual dos LEDs
 * @return O valor que está sendo exibido
 */
uint16_t hal_gpio_read(void);

/**
 * @brief Lê o estado dos Switches (se disponível)
 * @return Valor dos switches (16 bits) ou 0 se não suportado
 */
uint16_t hal_gpio_read_switches(void);

#endif // HAL_GPIO_H