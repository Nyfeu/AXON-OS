#include "../../../include/hal/hal_gpio.h"
#include "memory_map.h" 

// Estrutura mapeando os registradores do hardware
typedef struct {

    volatile uint32_t leds;      // Offset 0x00: RW (Read/Write)
    volatile uint32_t switches;  // Offset 0x04: RO (Read Only)

} gpio_hw_t;

#define GPIO ((gpio_hw_t*)GPIO_BASE)

void hal_gpio_init(void) {
    // Hardware físico não precisa de setup inicial neste caso,
    // mas poderíamos apagar os LEDs no boot.
    GPIO->leds = 0;
}

void hal_gpio_write(uint16_t val) {
    // O hardware pega apenas os 16 bits inferiores conforme gpio_controller.vhd
    GPIO->leds = (uint32_t)val;
}

uint16_t hal_gpio_read(void) {
    return (uint16_t)(GPIO->leds);
}

uint16_t hal_gpio_read_switches(void) {
    return (uint16_t)(GPIO->switches);
}