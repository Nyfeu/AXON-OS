#include "sys/syscall.h"
#include "hal/hal_gpio.h"
#include "apps/shell_utils.h"

// ======================================================================================
// TASK: LEDS 
// ======================================================================================

// Faz contagem binária nos LEDs e mostra status no shell

void task_leds(void) {
    uint32_t counter = 0;
    
    hal_gpio_init();

    while (1) {
        hal_gpio_write(counter);
        
        int led_on = counter % 2; 
        
        if (led_on) {
            safe_puts("\0337\033[1;70H\033[37m[LED: \033[1;32m(*)\033[0;37m]\0338");
        } else {
            safe_puts("\0337\033[1;70H\033[37m[LED: \033[1;31m( )\033[0;37m]\0338");
        }

        counter++;
        sys_sleep(500);
    }
}
