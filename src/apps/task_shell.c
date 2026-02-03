#include <stddef.h>
#include "sys/syscall.h"
#include "kernel/mutex.h"
#include "hal/hal_uart.h"
#include "util/circular_buffer.h"
#include "apps/commands.h"

// ======================================================================================
// Definições de CORES para o SHELL
// ======================================================================================

#define SH_RESET   "\033[0m"
#define SH_CYAN    "\033[36m"
#define SH_GREEN   "\033[32m"
#define SH_YELLOW  "\033[33m"
#define SH_RED     "\033[31m"
#define SH_BOLD    "\033[1m"
#define SH_GRAY    "\033[90m"

// Tamanho máximo do comando SHELL
#define CMD_MAX_LEN 64

// Mutex para uso da UART (definido em apps.c)
extern mutex_t uart_mutex;

// Buffer do teclado (definido em apps.c)
extern cbuf_t rx_buffer;

// ======================================================================================
// ISR (Interrupt Service Routine) 
// ======================================================================================

void uart_isr(void) {
    if (hal_uart_kbhit()) {
        char c = hal_uart_getc();
        cbuf_push(&rx_buffer, (uint8_t)c);
    }
}

// ======================================================================================
// HELPERS
// ======================================================================================

int sys_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void safe_puts(const char* s) {
    while (sys_mutex_lock(&uart_mutex) == 0) sys_yield(); 
    sys_puts(s);
    sys_mutex_unlock(&uart_mutex);
}

void show_prompt(void) {
    safe_puts(SH_GREEN "root@axon" SH_RESET ":" SH_CYAN "~$ " SH_RESET);
}

void clear_screen(void) {
    safe_puts("\033[2J\033[H\033[3;0H");
    safe_puts(SH_CYAN SH_BOLD);
    safe_puts("   AXON RTOS ");
    safe_puts(SH_RESET SH_GRAY " v0.1.0-alpha (RISC-V 32)\n" SH_RESET);
    safe_puts(SH_GRAY "   Type 'help' for commands.\n\n" SH_RESET);
}

// ======================================================================================
// TABELA DE COMANDOS (Command Registry)
// ======================================================================================

typedef struct {
    const char *name;
    void (*func)(const char *args); 
} shell_cmd_t;

static const shell_cmd_t shell_commands[] = {
    {"help",    cmd_help},
    {"clear",   cmd_clear},
    {"reboot",  cmd_reboot},
    {"panic",   cmd_panic},
    {"ps",      cmd_ps},
    {"memtest", cmd_memtest},
    {"heap",    cmd_heap},
    {"peek",    cmd_peek},
    {"poke",    cmd_poke},
    {"alloc",   cmd_alloc}, 
    {"stop",    cmd_stop},  
    {"resume",  cmd_resume},
    {"free",    cmd_free}
};

#define CMD_COUNT (sizeof(shell_commands) / sizeof(shell_cmd_t))

// ======================================================================================
// TASK: SHELL
// ======================================================================================

void task_shell(void) {

    uint8_t c;
    char cmd_buf[CMD_MAX_LEN];
    int cmd_idx = 0;

    safe_puts("\n"); 
    show_prompt();
    
    while (1) {
        
        if (cbuf_pop(&rx_buffer, &c)) {
            
            // --- CTRL+L (Form Feed) ---
            if (c == 12) { 
                
                safe_puts("\033[2J\033[H\033[3;0H");
                safe_puts(SH_CYAN SH_BOLD "   AXON RTOS " SH_RESET SH_GRAY " v0.1.0-alpha\n\n" SH_RESET);
                show_prompt();
                
                if (cmd_idx > 0) {
                    cmd_buf[cmd_idx] = 0;
                    safe_puts(cmd_buf);
                }

            }
            
            // --- ENTER ---
            else if (c == '\r') {
                cmd_buf[cmd_idx] = 0;
                safe_puts("\n");

                if (cmd_idx > 0) {
                    
                    // 1. Separação de Comando e Argumentos
                    char *cmd_name = cmd_buf;
                    char *args = NULL;

                    // Procura o primeiro espaço
                    for (int k = 0; cmd_buf[k]; k++) {
                        if (cmd_buf[k] == ' ') {
                            cmd_buf[k] = 0;         // Corta a string aqui (terminador nulo)
                            args = &cmd_buf[k + 1]; // O resto são argumentos
                            
                            // Pula espaços extras no início dos argumentos
                            while (*args == ' ') args++;
                            if (*args == 0) args = NULL; // Se só tinha espaços, args é NULL
                            
                            break;
                        }
                    }

                    // 2. Busca e Execução
                    int found = 0;
                    for (int i = 0; i < CMD_COUNT; i++) {
                        // Compara apenas o nome (agora isolado pelo \0 inserido acima)
                        if (sys_strcmp(cmd_name, shell_commands[i].name) == 0) {
                            // CORREÇÃO: Passa 'args' para a função!
                            shell_commands[i].func(args);
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        safe_puts(SH_RED "Unknown command: " SH_RESET);
                        safe_puts(cmd_name); safe_puts("\n");
                    }
                }
                show_prompt();
                cmd_idx = 0;
            }
            
            // --- BACKSPACE ---
            else if (c == 127 || c == '\b') {
                if (cmd_idx > 0) {
                    safe_puts("\b \b");
                    cmd_idx--;
                }
            }
            
            // --- CARACTERES IMPRIMÍVEIS ---
            else if (cmd_idx < (CMD_MAX_LEN - 1) && c >= 32 && c <= 126) {
                char s[2] = { (char)c, 0 };
                safe_puts(s);
                cmd_buf[cmd_idx++] = c;
            }
        } else {
            sys_sleep(10);
        }
    }
}
