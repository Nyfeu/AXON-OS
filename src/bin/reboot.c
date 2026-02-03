#include "../../include/shell_utils.h"

// ======================================================================================
// COMANDO: REBOOT
// ======================================================================================

void cmd_reboot(void) {
    safe_puts("Rebooting...\n");
    sys_sleep(500);
    extern void _start(void);
    _start();
}
