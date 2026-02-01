#include "../../include/syscall.h"
#include "../../include/mutex.h"
#include "../../include/apps.h"
#include "../../include/hal_uart.h" 
#include "../../include/hal_irq.h"  
#include "../../include/hal_plic.h" 
#include "../../include/circular_buffer.h"
#include "../../include/logger.h"

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
                        safe_puts(SH_BOLD "\n  PID   NAME        PRIO   STATE\n" SH_RESET);
                        safe_puts(SH_GRAY "  ------------------------------------\n" SH_RESET);
                        
                        // Nota: Como ainda não temos uma Syscall para ler a memória do Kernel,
                        // estes dados são estáticos (hardcoded) por enquanto.
                        safe_puts("  0     Idle        0      " SH_GREEN "RUNNING" SH_RESET "\n");
                        safe_puts("  1     Task A      1      " SH_YELLOW "WAITING" SH_RESET "\n");
                        safe_puts("  2     Task B      1      " SH_YELLOW "WAITING" SH_RESET "\n");
                        safe_puts("  3     Shell       2      " SH_GREEN "RUNNING" SH_RESET "\n");
                        safe_puts("\n");
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