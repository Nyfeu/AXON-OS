#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: REBOOT
// ======================================================================================

void cmd_reboot(const char *args) {
    (void)args; // Ignora os argumentos
    safe_puts("Rebooting...\n");
    sys_sleep(500);
    extern void _start(void);
    _start();
}
