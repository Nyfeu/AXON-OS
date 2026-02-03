#include "../../include/shell_utils.h"

// ======================================================================================
// COMANDO: HELP
// ======================================================================================

void cmd_help(void) {
    safe_puts(SH_BOLD "\n  AVAILABLE COMMANDS\n" SH_RESET);
    safe_puts(SH_GRAY "  ------------------------------------------\n" SH_RESET);
    safe_puts("  " SH_CYAN "help   " SH_RESET " Show this information\n");
    safe_puts("  " SH_CYAN "clear  " SH_RESET " Clear screen (keeps status bar)\n");
    safe_puts("  " SH_CYAN "ps     " SH_RESET " Show process status\n");
    safe_puts("  " SH_CYAN "memtest" SH_RESET " Test Heap Allocation\n");
    safe_puts("  " SH_CYAN "reboot " SH_RESET " Reboot the system\n");
    safe_puts("  " SH_CYAN "panic  " SH_RESET " Trigger a Kernel Panic\n\n");
}
