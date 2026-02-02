#include "../../include/syscall.h"
#include "../../include/mutex.h"
#include "../../include/apps.h"
#include "../../include/hal_uart.h" 
#include "../../include/hal_irq.h"  
#include "../../include/hal_plic.h" 
#include "../../include/circular_buffer.h"
#include "../../include/logger.h"
#include "../../include/mm.h"

// Mutex para uso da UART
mutex_t uart_mutex;

// Buffer do teclado
cbuf_t  rx_buffer;

// Tamanho máximo do comando SHELL
#define CMD_MAX_LEN 64

// Endereço base do sistema (RAM)
extern void _start(void);

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

// ======================================================================================
// ISR (Interrupt Service Routine) 
// ======================================================================================

// Esta função roda em contexto de INTERRUPÇÃO (prioridade máxima).
// Ela deve ser rápida! Só pega o dado e guarda.

void uart_isr(void) {

    // Verificação de Segurança: só tente ler se tiver dados
    if (hal_uart_kbhit()) {

        // 1. Lê o char do hardware (isso limpa o flag de IRQ na UART)
        char c = hal_uart_getc();
        
        // 2. Guarda no buffer (se houver espaço)
        cbuf_push(&rx_buffer, (uint8_t)c);
        
    }
 
}
// ======================================================================================
// HELPERS
// ======================================================================================

int sys_strcmp(const char *s1, const char *s2) {

    // Retorna 0 se as strings forem iguais.
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;

}

void safe_puts(const char* s) {

    // Tenta pegar a chave da UART
    // Se estiver ocupado (retorna 0), cede a vez (yield) e tenta de novo depois.
    // Isso cria um "Spinlock Cooperativo"
    while (sys_mutex_lock(&uart_mutex) == 0)  sys_yield(); 
    
    // ------- REGIÃO CRÍTICA -------------------------------------------------------

    // Tendo conseguido pegar a chave, agora, somos donos exclusivos da UART.
    // Ninguém mais escreve além de nós.
    sys_puts(s);

    // ------------------------------------------------------------------------------

    // Devolve a chave para a próxima tarefa usar.
    sys_mutex_unlock(&uart_mutex);

}

void show_prompt(void) {
    // Prompt colorido: "user@axon:~$ "
    safe_puts(SH_GREEN "root@axon" SH_RESET ":" SH_CYAN "~$ " SH_RESET);
}

void clear_screen(void) {

    safe_puts("\033[2J\033[H");
    
    // Banner Bonito
    safe_puts("\n");
    safe_puts(SH_CYAN SH_BOLD);
    safe_puts("   AXON RTOS ");
    safe_puts(SH_RESET SH_GRAY " v0.1.0-alpha (RISC-V 32)\n" SH_RESET);
    safe_puts(SH_GRAY "   Type 'help' for commands.\n\n" SH_RESET);

}

void val_to_hex(uint32_t val, char* out_buf) {
    const char hex_chars[] = "0123456789ABCDEF";
    out_buf[0] = '0'; 
    out_buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        out_buf[2 + (7-i)] = hex_chars[(val >> (i*4)) & 0xF];
    }
    out_buf[10] = 0; // Null terminator
}

// ======================================================================================
//  ESPAÇO DO USUÁRIO 
// ======================================================================================
//
//  Estas funções representam programas rodando no SO.
//  Note que elas NÃO acessam hardware diretamente (nada de mmio_write).
//  Elas usam as syscalls para pedir favores ao Kernel.
//
// ======================================================================================

// Tarefa A: Um cidadão comportado
void task_a(void) {
    while (1) {

        // Pede para imprimir a string
        // safe_puts("Hello, ");

        // Pede ao kernel para dormir (Syscall 3).
        // Isso coloca a tarefa em estado BLOCKED e libera a CPU imediatamente.
        sys_sleep(500);

    }

}

// Tarefa B: Outro cidadão comportado
void task_b(void) {
    while (1) {
        
        // Pede para imprimir a string
        // safe_puts("World!\n\r");

        // Pede ao kernel para dormir (Syscall 3).
        // Isso coloca a tarefa em estado BLOCKED e libera a CPU imediatamente.
        sys_sleep(500);

    }
}

// Tarefa SHELL: Reponsável por leitura do teclado
void task_shell(void) {
    uint8_t c;
    char cmd_buf[CMD_MAX_LEN];
    int cmd_idx = 0;

    show_prompt();
    
    while (1) {

        if (cbuf_pop(&rx_buffer, &c)) {
            
            // 1. CTRL+L (Clear)
            if (c == 12) {
                clear_screen();
                show_prompt();
                cmd_buf[cmd_idx] = 0;
                safe_puts(cmd_buf);
            }
            
            // 2. ENTER (Use 'else if' para não cair nos outros!)
            else if (c == '\r') {
                cmd_buf[cmd_idx] = 0;
                safe_puts("\n");

                if (cmd_idx > 0) {

                    // --- COMANDO HELP ---
                    if (sys_strcmp(cmd_buf, "help") == 0) {
                        safe_puts(SH_BOLD "\n  AVAILABLE COMMANDS\n" SH_RESET);
                        safe_puts(SH_GRAY "  ------------------------------------------\n" SH_RESET);
                        safe_puts("  " SH_CYAN "help  " SH_RESET "  Show this information\n");
                        safe_puts("  " SH_CYAN "clear " SH_RESET "  Clear the terminal screen\n");
                        safe_puts("  " SH_CYAN "ps    " SH_RESET "  Show process status (Tasks)\n");
                        safe_puts("  " SH_CYAN "reboot" SH_RESET "  Reboot the system\n");
                        safe_puts("  " SH_CYAN "panic " SH_RESET "  Trigger a Kernel Panic test\n");
                        safe_puts("\n");
                    }

                    // --- COMANDO CLEAR ---
                    else if (sys_strcmp(cmd_buf, "clear") == 0) {
                        clear_screen();
                    }

                    // --- COMANDO REBOOT ---
                    else if (sys_strcmp(cmd_buf, "reboot") == 0) {
                        safe_puts("Rebooting...\n");
                        sys_sleep(500); // Aqui pode usar sys_sleep (estamos em User Mode)
                        _start();
                    }

                    // --- COMANDO PANIC ---
                    else if (sys_strcmp(cmd_buf, "panic") == 0) {
                        safe_puts("Triggering Illegal Instruction...\n");
                        asm volatile(".word 0xFFFFFFFF");
                    }

                    // --- COMANDO PS (Process Status) ---
                    else if (sys_strcmp(cmd_buf, "ps") == 0) {
                        task_info_t list[8]; 
                        int count = sys_get_tasks(list, 8);
                        
                        // Cabeçalho expandido
                        safe_puts(SH_BOLD "\n  PID   NAME            PRIO   STATE      SP          WAKE_TIME\n" SH_RESET);
                        safe_puts(SH_GRAY "  ----------------------------------------------------------------\n" SH_RESET);
                        
                        for (int i = 0; i < count; i++) {
                            // 1. PID
                            char pid_str[4];
                            pid_str[0] = list[i].id + '0'; 
                            pid_str[1] = 0;
                            
                            // 2. STATE
                            const char *state_str;
                            switch(list[i].state) {
                                case 0: state_str = SH_GREEN "READY  " SH_RESET; break;
                                case 1: state_str = SH_CYAN  "RUNNING" SH_RESET; break;
                                case 2: state_str = SH_YELLOW "WAITING" SH_RESET; break;
                                default: state_str = "UNKNOWN"; break;
                            }

                            // 3. SP (Stack Pointer) em Hex
                            char sp_str[12];
                            val_to_hex(list[i].sp, sp_str);

                            // 4. Wake Time (apenas os 32 bits baixos para caber na tela)
                            char wake_str[12];
                            val_to_hex((uint32_t)list[i].wake_time, wake_str);

                            // --- IMPRESSÃO FORMATADA ---
                            safe_puts("  ");
                            safe_puts(pid_str);
                            safe_puts("     ");
                            
                            // Nome alinhado
                            safe_puts(list[i].name);
                            int len = 0; while(list[i].name[len]) len++;
                            for(int s=0; s<(16-len); s++) safe_puts(" ");
                            
                            // Prioridade
                            if (list[i].priority == 0) safe_puts("0      ");
                            else safe_puts("1      ");
                            
                            // Estado
                            safe_puts(state_str);
                            safe_puts("    ");
                            
                            // SP
                            safe_puts(sp_str);
                            safe_puts("  ");
                            
                            // Wake Time (Se for 0 ou muito antigo, mostra tracinho)
                            if (list[i].state != 2) safe_puts("-         ");
                            else safe_puts(wake_str);
                            
                            safe_puts("\n");
                        }
                        safe_puts("\n");
                    }

                    // -- COMANDO MEMTEST ---
                    else if (sys_strcmp(cmd_buf, "memtest") == 0) {
                        safe_puts("Allocating 128 bytes on Heap...\n");
                        
                        // 1. Aloca
                        char *ptr = (char*)kmalloc(128);
                        
                        if (ptr) {
                            safe_puts("Success! Addr: ");
                            // Imprime endereço 
                            char addr_str[12];
                            val_to_hex((uint32_t)ptr, addr_str);
                            safe_puts(addr_str);
                            safe_puts("\n");
                            
                            // 2. Usa (Escreve na memória)
                            ptr[0] = 'H'; ptr[1] = 'e'; ptr[2] = 'y'; ptr[3] = '!'; ptr[4] = 0;
                            safe_puts("Data written: "); safe_puts(ptr); safe_puts("\n");
                            
                            // 3. Libera
                            safe_puts("Freeing memory...\n\n");
                            kfree(ptr);
                        } else {
                            safe_puts(SH_RED "Malloc failed (OOM)!\n\n" SH_RESET);
                        }
                    }

                    // -- COMANDO DESCONHECIDO ---
                    else {
                        safe_puts("Unknown: ");
                        safe_puts(cmd_buf);
                        safe_puts("\n");
                    }
                    
                }

                show_prompt();
                cmd_idx = 0;

            }
            
            // 3. BACKSPACE
            else if (c == 127 || c == '\b') {
                if (cmd_idx > 0) {
                    safe_puts("\b \b");
                    cmd_idx--;
                }
            }
            
            // 4. CARACTERE NORMAL (Só entra aqui se não foi nenhum dos acima)
            else {
                if (cmd_idx < (CMD_MAX_LEN - 1) && c >= 32 && c <= 126) {
                    char s[2] = { (char)c, 0 };
                    safe_puts(s);
                    cmd_buf[cmd_idx++] = c;
                }
            }
        } else {
            sys_yield(); 
        }
    }
}

// Inicialização da aplicação 
void apps_init(void) {
    
    mutex_init(&uart_mutex);
    cbuf_init(&rx_buffer);

    uint8_t plic_uart_id = get_uart_irq_id();

    // 1. Registra a função 'uart_isr' para o ID da UART
    hal_irq_register(plic_uart_id, uart_isr);
    
    // 2. Habilita a interrupção no PLIC
    hal_plic_set_priority(plic_uart_id, 1);
    hal_plic_enable(plic_uart_id);

}