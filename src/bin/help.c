#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: HELP
// ======================================================================================

void cmd_help(const char *args) {
    (void)args; // Ignora os argumentos

    // Cabeçalho
    safe_puts(SH_BOLD "\n  AVAILABLE COMMANDS\n" SH_RESET);
    safe_puts(SH_GRAY "  ------------------------------------------\n" SH_RESET);

    // Sistema
    safe_puts("  " SH_CYAN "help      " SH_RESET " Show this information\n");
    safe_puts("  " SH_CYAN "clear     " SH_RESET " Clear screen\n");
    safe_puts("  " SH_CYAN "reboot    " SH_RESET " Reboot system\n");
    safe_puts("  " SH_CYAN "panic     " SH_RESET " Trigger Kernel Panic\n");
    
    // Processos
    safe_puts("  " SH_CYAN "ps        " SH_RESET " Process status\n");
    safe_puts("  " SH_CYAN "stop      " SH_RESET " Suspend task (stop <pid>)\n");
    safe_puts("  " SH_CYAN "resume    " SH_RESET " Resume task (resume <pid>)\n");
    
    // Memória
    safe_puts("  " SH_CYAN "heap      " SH_RESET " Show heap usage\n");
    safe_puts("  " SH_CYAN "alloc     " SH_RESET " Safe alloc (alloc <bytes>)\n");
    safe_puts("  " SH_CYAN "peek      " SH_RESET " Read memory (peek <addr>)\n");
    safe_puts("  " SH_CYAN "poke      " SH_RESET " Write memory (poke <addr> <val>)\n");
    safe_puts("  " SH_CYAN "memtest   " SH_RESET " Simple malloc test\n");
    safe_puts("  " SH_CYAN "free      " SH_RESET " Free memory (free <addr>)\n");

    safe_puts("\n");

}
