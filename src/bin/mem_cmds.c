#include "apps/commands.h"
#include "sys/syscall.h"
#include "apps/shell_utils.h" 

// Helper para converter Hex String para Uint32
static uint32_t strtoul_hex(const char *s) {
    uint32_t val = 0;
    if (!s) return 0;
    
    // Pula "0x" se existir
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;

    while (*s) {
        uint8_t byte = (uint8_t)*s;
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;
        else break;
        val = (val << 4) | byte;
        s++;
    }
    return val;
}

void cmd_heap(const char *args) {
    (void)args;
    sys_heap_info(); // Syscall que chama kheap_dump()
}

void cmd_peek(const char *args) {
    if (!args || !*args) {
        safe_puts("Usage: peek <addr>\n");
        return;
    }

    uint32_t addr = strtoul_hex(args);
    uint32_t val = sys_peek(addr);

    safe_puts("Read [");
    // (Aqui você precisaria de uma função print_hex no shell_utils ou manual)
    // Exemplo simplificado manual:
    char buf[12]; val_to_hex(addr, buf); safe_puts(buf);
    safe_puts("] = ");
    val_to_hex(val, buf); safe_puts(buf);
    safe_puts("\n");
}

void cmd_poke(const char *args) {
    if (!args || !*args) {
        safe_puts("Usage: poke <addr> <val>\n");
        return;
    }

    // Parser manual simples para separar "ADDR VAL"
    char arg1[32];
    char arg2[32];
    
    int i = 0;
    const char *p = args;
    
    // Copia addr
    while (*p && *p != ' ' && i < 31) arg1[i++] = *p++;
    arg1[i] = 0;
    
    // Pula espaços
    while (*p && *p == ' ') p++;
    
    // Copia val
    i = 0;
    while (*p && *p != ' ' && i < 31) arg2[i++] = *p++;
    arg2[i] = 0;

    if (i == 0) {
        safe_puts("Usage: poke <addr> <val>\n");
        return;
    }

    uint32_t addr = strtoul_hex(arg1);
    uint32_t val  = strtoul_hex(arg2);

    sys_poke(addr, val);
    safe_puts("Written.\n");
}

void cmd_free(const char *args) {
    if (!args || !*args) {
        safe_puts("Usage: free <addr>\n");
        return;
    }

    // Usa a mesma função auxiliar de hex que usamos no peek/poke
    // (Assumindo que strtoul_hex ou similar está disponível aqui)
    uint32_t addr = strtoul_hex(args);
    
    // Chama o Kernel
    uint8_t res = sys_free((void*)addr);
    
    if (res == 0) {
        safe_puts("Freed block at 0x");
        char buf[12]; val_to_hex(addr, buf); safe_puts(buf);
        safe_puts("\n");
    } else {
        safe_puts(SH_RED "Failed to free block.\n" SH_RESET);
    }

}

void cmd_defrag(const char *args) {
    (void)args; // Ignora argumentos
    sys_defrag(); // Chama a syscall
    safe_puts("Heap defragmentation requested.\n");
}