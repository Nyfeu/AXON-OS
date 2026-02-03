#include "../../include/syscall.h"
#include "../../include/mutex.h"
#include "../../include/apps.h"
#include "../../include/hal_uart.h" 
#include "../../include/hal_irq.h"  
#include "../../include/hal_plic.h" 
#include "../../include/circular_buffer.h"
#include "../../include/logger.h"
#include "../../include/mm.h"
#include "../../include/hal_gpio.h"

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

    safe_puts("\033[2J\033[H\033[3;0H");
    
    // Banner do sistema
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

void int_to_str(int val, char* buf) {
    // Conversão simples para inteiros positivos pequenos (0-99)
    if (val < 10) {
        buf[0] = '0';
        buf[1] = val + '0';
        buf[2] = 0;
    } else {
        buf[0] = (val / 10) + '0';
        buf[1] = (val % 10) + '0';
        buf[2] = 0;
    }
}

// ======================================================================================
// IMPLEMENTAÇÃO DOS COMANDOS DO SHELL
// ======================================================================================

void cmd_help(void) {
    safe_puts(SH_BOLD "\n  AVAILABLE COMMANDS\n" SH_RESET);
    safe_puts(SH_GRAY "  ------------------------------------------\n" SH_RESET);
    safe_puts("  " SH_CYAN "help   " SH_RESET " Show this information\n");
    safe_puts("  " SH_CYAN "clear  " SH_RESET " Clear screen (keeps status bar)\n");
    safe_puts("  " SH_CYAN "ps     " SH_RESET " Show process status\n");
    safe_puts("  " SH_CYAN "memtest" SH_RESET " Test Heap Allocation\n");
    safe_puts("  " SH_CYAN "reboot " SH_RESET " Reboot the system\n");
    safe_puts("  " SH_CYAN "panic  " SH_RESET " Trigger a Kernel Panic\n\n");
}

void cmd_clear(void) {
    // Limpa a tela mas move o cursor para a linha 3 (pulando a barra de status)
    safe_puts("\033[2J\033[3;1H"); 
    safe_puts(SH_CYAN SH_BOLD "   AXON RTOS " SH_RESET SH_GRAY " v0.1.0-alpha\n\n" SH_RESET);
}

void cmd_reboot(void) {
    safe_puts("Rebooting...\n");
    sys_sleep(500);
    extern void _start(void);
    _start();
}

void cmd_panic(void) {
    safe_puts("Triggering Illegal Instruction...\n");
    asm volatile(".word 0xFFFFFFFF");
}

void cmd_ps(void) {
    task_info_t list[8]; 
    int count = sys_get_tasks(list, 8);
    
    safe_puts(SH_BOLD "\n  PID   NAME            PRIO   STATE      SP          WAKE_TIME\n" SH_RESET);
    safe_puts(SH_GRAY "  ----------------------------------------------------------------\n" SH_RESET);
    
    for (int i = 0; i < count; i++) {
        char pid_str[4]; pid_str[0] = list[i].id + '0'; pid_str[1] = 0;
        
        const char *state_str;
        switch(list[i].state) {
            case 0: state_str = SH_GREEN "READY  " SH_RESET; break;
            case 1: state_str = SH_CYAN  "RUNNING" SH_RESET; break;
            case 2: state_str = SH_YELLOW "WAITING" SH_RESET; break;
            default: state_str = "UNKNOWN"; break;
        }

        char sp_str[12], wake_str[12];
        val_to_hex(list[i].sp, sp_str);
        val_to_hex((uint32_t)list[i].wake_time, wake_str);

        safe_puts("  "); safe_puts(pid_str); safe_puts("     ");
        safe_puts(list[i].name);
        
        // Alinhamento
        int len = 0; while(list[i].name[len]) len++;
        for(int s=0; s<(16-len); s++) safe_puts(" ");
        
        safe_puts(list[i].priority ? "1      " : "0      ");
        safe_puts(state_str); safe_puts("    ");
        safe_puts(sp_str); safe_puts("  ");
        safe_puts(list[i].state != 2 ? "-         " : wake_str);
        safe_puts("\n");
    }
    safe_puts("\n");
}

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

// ======================================================================================
// TABELA DE COMANDOS (Command Registry)
// ======================================================================================

typedef struct {

    const char *name;
    void (*func)(void);

} shell_cmd_t;

static const shell_cmd_t shell_commands[] = {
    {"help",    cmd_help},
    {"clear",   cmd_clear},
    {"reboot",  cmd_reboot},
    {"panic",   cmd_panic},
    {"ps",      cmd_ps},
    {"memtest", cmd_memtest}
};

#define CMD_COUNT (sizeof(shell_commands) / sizeof(shell_cmd_t))

// ======================================================================================
//  ESPAÇO DO USUÁRIO 
// ======================================================================================
//
//  Estas funções representam programas rodando no SO.
//  Note que elas NÃO acessam hardware diretamente (nada de mmio_write).
//  Elas usam as syscalls para pedir favores ao Kernel.
//
// ======================================================================================

// Tarefa A: Controla os LEDs
void task_a(void) {

    uint32_t counter = 0;
    
    // Inicialização do GPIO
    hal_gpio_init();

    while (1) {

        // 1. Atualiza Hardware (Funciona na FPGA)
        hal_gpio_write(counter);
        
        // 2. Feedback Visual no QEMU
        // Verifica o bit 0 do contador para simular um LED piscando
        int led_on = counter % 2; 
        
        // Sequência ANSI:
        // \0337      -> Salva Posição do Cursor e Atributos (DEC Save Cursor)
        // \033[1;70H -> Move cursor para Linha 1, Coluna 60 (Canto superior direito)

        // Imprime o status
        // \0338      -> Restaura Posição do Cursor e Atributos (DEC Restore Cursor)
        
        if (led_on) {
            // [LED: (*)] em Verde
            safe_puts("\0337\033[1;70H\033[37m[LED: \033[1;32m(*)\033[0;37m]\0338");
        } else {
            // [LED: ( )] em Vermelho/Apagado
            safe_puts("\0337\033[1;70H\033[37m[LED: \033[1;31m( )\033[0;37m]\0338");
        }

        counter++;
        
        // Delay para a tarefa não rodar rápido demais
        sys_sleep(500);

    }

}

// Tarefa B: Exibe Uptime e Spinner
void task_b(void) {

    // Contadores de tempo
    int seconds = 0;
    int minutes = 0;

    // Buffers para formatação
    char s_str[4], m_str[4];
    
    // Spinner para mostrar atividade
    const char spinner[] = "|/-\\";
    int spin_idx = 0;

    while (1) {

        // Formata o tempo
        int_to_str(minutes, m_str);
        int_to_str(seconds, s_str);

        // Monta a barra de status na Linha 1, Coluna 1
        // Formato: Uptime: MM:SS | <spinner>
        
        // Sequência ANSI:
        // \0337      = Save Cursor
        // \033[1;1H  = Move to 1,1
        // Imprime texto
        // \0338      = Restore Cursor
        
        // Pede a chave da UART (mutex)
        sys_mutex_lock(&uart_mutex); 
        
        // Move o cursor para o topo
        sys_puts("\0337\033[1;1H"); 

        // Exibe a barra de status (uptime + spinner)
        sys_puts("Uptime: ");
        sys_puts(m_str); sys_puts(":"); sys_puts(s_str);
        sys_puts("  ["); 
        char spin_char[2] = { spinner[spin_idx], 0 };
        sys_puts(SH_CYAN); sys_puts(spin_char); sys_puts(SH_RESET);
        sys_puts("]");

        // Volta para onde o usuário estava digitando
        sys_puts("\0338");
        
        // Devolve a chave da UART (mutex)
        sys_mutex_unlock(&uart_mutex);

        // Atualiza contadores
        spin_idx = (spin_idx + 1) % 4;
        
        // Rodando a cada 250ms para o spinner girar
        // A cada 4 ciclos (1000ms), incrementa o segundo
        if (spin_idx == 0) {
            seconds++;
            if (seconds >= 60) {
                seconds = 0;
                minutes++;
            }
        }

        // A tarefa dorme por 250 ms
        sys_sleep(250);

    }
    
}

// Tarefa SHELL: Reponsável por leitura do teclado
void task_shell(void) {
    uint8_t c;
    char cmd_buf[CMD_MAX_LEN];
    int cmd_idx = 0;

    // CORREÇÃO 1: Não limpar a tela no boot. 
    // Apenas pulamos uma linha para separar dos logs do kernel.
    safe_puts("\n"); 
    show_prompt();
    
    while (1) {
        if (cbuf_pop(&rx_buffer, &c)) {
            
            // --- CTRL+L (Form Feed) ---
            // CORREÇÃO 2: Reimplementação do atalho de limpar tela mantendo o texto
            if (c == 12) { 
                cmd_clear();       // Limpa a tela (preservando barra de status)
                show_prompt();     // Redesenha "root@axon:~$"
                
                // Redesenha o que o usuário já tinha digitado
                if (cmd_idx > 0) {
                    cmd_buf[cmd_idx] = 0; // Garante terminador nulo
                    safe_puts(cmd_buf);
                }
            }
            
            // --- ENTER ---
            else if (c == '\r') {
                cmd_buf[cmd_idx] = 0;
                safe_puts("\n");

                if (cmd_idx > 0) {
                    int found = 0;
                    for (int i = 0; i < CMD_COUNT; i++) {
                        if (sys_strcmp(cmd_buf, shell_commands[i].name) == 0) {
                            shell_commands[i].func();
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        safe_puts(SH_RED "Unknown command: " SH_RESET);
                        safe_puts(cmd_buf); safe_puts("\n");
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