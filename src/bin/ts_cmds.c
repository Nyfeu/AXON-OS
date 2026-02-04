#include "apps/commands.h"
#include "sys/syscall.h"
#include "apps/shell_utils.h"

// Helper simples para string -> int
static uint32_t my_atoi(const char *s) {
    uint32_t n = 0;
    while(*s >= '0' && *s <= '9') { n = n*10 + (*s++ - '0'); }
    return n;
}

void cmd_stop(const char *args) {
    if(!args) { safe_puts("Usage: stop <pid>\n"); return; }
    uint32_t pid = my_atoi(args);
    if(sys_suspend(pid) == 0) safe_puts("Task suspended.\n");
    else safe_puts("Error.\n");
}

void cmd_resume(const char *args) {
    if(!args) { safe_puts("Usage: cont <pid>\n"); return; }
    uint32_t pid = my_atoi(args);
    if(sys_resume(pid) == 0) safe_puts("Task resumed.\n");
    else safe_puts("Error.\n");
}

// Memória Segura: O usuário pede um bloco, o kernel dá o endereço.
// O usuário então usa 'poke' nesse endereço sabendo que é dele.
void cmd_alloc(const char *args) {
    if(!args) { safe_puts("Usage: alloc <bytes>\n"); return; }
    uint32_t size = my_atoi(args);
    
    void* ptr = sys_malloc(size);
    if(ptr) {
        safe_puts("Allocated at: ");
        char buf[12]; val_to_hex((uint32_t)ptr, buf); safe_puts(buf);
        safe_puts("\nUse 'poke <addr> <val>' to write safely.\n\n");
    } else {
        safe_puts("Allocation failed (OOM).\n");
    }
}