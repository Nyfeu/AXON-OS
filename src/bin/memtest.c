#include "apps/shell_utils.h"
#include "kernel/mm.h"

// ======================================================================================
// COMANDO: MEMTEST
// ======================================================================================

void cmd_memtest(void) {
    safe_puts("Allocating 128 bytes on Heap...\n");
    char *ptr = (char*)kmalloc(128);
    if (ptr) {
        char addr_str[12]; val_to_hex((uint32_t)ptr, addr_str);
        safe_puts("Success! Addr: "); safe_puts(addr_str); safe_puts("\n");
        kfree(ptr);
    } else {
        safe_puts(SH_RED "Malloc failed (OOM)!\n" SH_RESET);
    }
}
