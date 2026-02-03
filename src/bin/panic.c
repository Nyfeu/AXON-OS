#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: PANIC
// ======================================================================================

void cmd_panic(void) {
    safe_puts("Triggering Illegal Instruction...\n");
    asm volatile(".word 0xFFFFFFFF");
}
