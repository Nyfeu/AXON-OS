#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: PANIC
// ======================================================================================

void cmd_panic(const char *args) {
    (void)args; // Ignora os argumentos
    safe_puts("Triggering Illegal Instruction...\n");
    asm volatile(".word 0xFFFFFFFF");
}
