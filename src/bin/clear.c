#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: CLEAR
// ======================================================================================

void cmd_clear(void) {
    safe_puts("\033[2J\033[3;1H"); 
    safe_puts(SH_CYAN SH_BOLD "   AXON RTOS " SH_RESET SH_GRAY " v0.1.0-alpha\n\n" SH_RESET);
}
