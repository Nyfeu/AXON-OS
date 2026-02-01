#include "../../include/syscall.h"
#include "../../include/mutex.h"
#include "../../include/apps.h"

// Mutex para uso da UART
mutex_t uart_mutex;

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

        // Tenta pegar a chave da UART
        // Se estiver ocupado (retorna 0), cede a vez (yield) e tenta de novo depois.
        // Isso cria um "Spinlock Cooperativo"
        while (sys_mutex_lock(&uart_mutex) == 0)  sys_yield(); 
        
        // ------- REGIÃO CRÍTICA -------------------------------------------------------

        // Tendo conseguido pegar a chave, agora, somos donos exclusivos da UART.
        // Ninguém mais escreve além de nós.
        sys_puts("Hello, ");

        // ------------------------------------------------------------------------------

        // Devolve a chave para a próxima tarefa usar.
        sys_mutex_unlock(&uart_mutex);

        // Pede ao kernel para dormir (Syscall 3).
        // Isso coloca a tarefa em estado BLOCKED e libera a CPU imediatamente.
        sys_sleep(500);

    }

}

// Tarefa B: Outro cidadão comportado
void task_b(void) {
    while (1) {
        
        while (sys_mutex_lock(&uart_mutex) == 0)  sys_yield(); 
        sys_puts("World!\n\r");
        sys_mutex_unlock(&uart_mutex);
        sys_sleep(500);

    }
}

// Inicialização da aplicação 
void apps_init(void) {
    mutex_init(&uart_mutex);
}