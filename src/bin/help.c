#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: HELP
// ======================================================================================

void cmd_help(const char *args) {
    (void)args; // Ignora os argumentos

    safe_puts(SH_BOLD "\n  AVAILABLE COMMANDS\n" SH_RESET);
    safe_puts(SH_GRAY "  ------------------------------------------\n" SH_RESET);
    safe_puts("  " SH_CYAN "help   " SH_RESET " Show this information\n");
    safe_puts("  " SH_CYAN "clear  " SH_RESET " Clear screen (keeps status bar)\n");
    safe_puts("  " SH_CYAN "ps     " SH_RESET " Show process status\n");
    safe_puts("  " SH_CYAN "memtest" SH_RESET " Test Heap Allocation\n");
    safe_puts("  " SH_CYAN "heap   " SH_RESET " Show heap memory usage\n");
    safe_puts("  " SH_CYAN "peek   " SH_RESET " Read memory (peek <addr>)\n");
    safe_puts("  " SH_CYAN "poke   " SH_RESET " Write memory (poke <addr> <val>)\n");
    safe_puts("  " SH_CYAN "reboot " SH_RESET " Reboot the system\n");
    safe_puts("  " SH_CYAN "panic  " SH_RESET " Trigger a Kernel Panic\n\n");
    
}
