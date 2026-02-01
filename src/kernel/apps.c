#include "../../include/syscall.h"
#include "../../include/mutex.h"
#include "../../include/apps.h"
#include "../../include/hal_uart.h" 
#include "../../include/hal_irq.h"  
#include "../../include/hal_plic.h" 
#include "../../include/circular_buffer.h"

// Mutex para uso da UART
mutex_t uart_mutex;

// Buffer do teclado
cbuf_t  rx_buffer;

// Tamanho máximo do comando SHELL
#define CMD_MAX_LEN 64

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
// HELPER DE SAÍDA
// ======================================================================================

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
    char cmd_buf[CMD_MAX_LEN];   // Buffer para guardar o que foi digitado
    int cmd_idx = 0;             // Posição atual do cursor

    safe_puts("\n[SHELL] Ready. Type something:\n> ");
    
    while (1) {

        // Tenta pegar dado do buffer circular
        if (cbuf_pop(&rx_buffer, &c)) {
            
            // --- TRATAMENTO DE TECLAS ESPECIAIS ---

            // 1. ENTER (Carriage Return '\r')
            if (c == '\r') {
                cmd_buf[cmd_idx] = 0; // Finaliza a string (Null Terminator)
                
                safe_puts("\n");      // Pula linha visualmente
                
                // Se o comando não for vazio, executa (simulação)
                if (cmd_idx > 0) {
                    safe_puts("[CMD] Executing: ");
                    safe_puts(cmd_buf);
                    safe_puts("\n");
                    
                    // Aqui colocariamos um "if (strcmp(cmd_buf, "help") == 0)..."
                }

                // Reseta para o próximo comando
                safe_puts("> ");
                cmd_idx = 0;

            }
            
            // 2. BACKSPACE (ASCII 127 ou 8)
            else if (c == 127 || c == '\b') {
                if (cmd_idx > 0) {
                    // Lógica Visual:
                    // 1. '\b': Move o cursor para a esquerda (em cima do caractere errado)
                    // 2. ' ':  Sobrescreve com espaço (apaga visualmente)
                    // 3. '\b': Move para a esquerda de novo (para ficar pronto para escrever)
                    safe_puts("\b \b");
                    
                    // Lógica de Memória:
                    cmd_idx--; // Remove do buffer
                }
            }
            
            // --- CARACTERE NORMAL ---
            else {

                // Proteção contra Buffer Overflow (não deixa digitar mais que 64 chars)
                if (cmd_idx < (CMD_MAX_LEN - 1)) {
                    // Echo Visual (Obrigatório para o usuário ver o que digita)
                    char s[2] = { (char)c, 0 };
                    safe_puts(s);
                    
                    // Guarda na memória
                    cmd_buf[cmd_idx++] = c;
                }

            }

        } else {
            // Se não tem tecla, cede a CPU
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