#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: CLEAR
// ======================================================================================

void cmd_clear(const char *args) {
    (void)args; // Ignora os argumentos
    clear_screen();
}
