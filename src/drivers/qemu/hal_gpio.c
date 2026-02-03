#include "hal_gpio.h"
#include "logger.h" // Opcional, para debug

// Variável estática para simular o estado dos LEDs na memória
static uint16_t virtual_leds = 0;

void hal_gpio_init(void) {
    // Nada a fazer no QEMU
}

void hal_gpio_write(uint16_t val) {
    virtual_leds = val;
    // Opcional: Imprimir no log se quiser ver o que estaria acontecendo
    // log_debug("GPIO: LEDs set to 0x%x", val);
}

uint16_t hal_gpio_read(void) {
    return virtual_leds;
}

uint16_t hal_gpio_read_switches(void) {
    // Retorna 0 pois não temos switches físicos emulados
    return 0;
}