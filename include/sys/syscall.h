#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "kernel/mutex.h"

// ==========================================================================================================
//  TABELA DE NÚMEROS DE SYSCALL
// ==========================================================================================================

// Estes números são o "código do pedido". O Kernel usa um switch/case no 
// trap_handler para saber qual serviço executar.

#define SYS_YIELD       1   // Ceder a CPU voluntariamente
#define SYS_WRITE       2   // Escrever no console (UART)
#define SYS_SLEEP       3   // Dormir por N milissegundos
#define SYS_LOCK        4   // Tentar pegar a chave
#define SYS_UNLOCK      5   // Devolver a chave
#define SYS_GET_TASKS   6   // Informações das tasks no sistema
#define SYS_PEEK        7   // Lê endereço de memória
#define SYS_POKE        8   // Escreve endereço de memória
#define SYS_HEAP_INFO   9   // Dump do Heap
#define SYS_MALLOC      10  // Para alocar memória segura
#define SYS_FREE        11  // Para liberar
#define SYS_DEFRAG      12  // Desfragmentar o heap
#define SYS_SUSPEND     13  // Pausar processo
#define SYS_RESUME      14  // Retomar processo
#define SYS_FS_CREATE   15  // Criar arquivo no sistema de arquivos
#define SYS_FS_WRITE    16  // Escrever arquivo no sistema de arquivos
#define SYS_FS_READ     17  // Ler arquivo do sistema de arquivos
#define SYS_FS_LIST     18  // Listar arquivos no sistema de arquivos
#define SYS_FS_DELETE   19  // Deletar arquivo do sistema de arquivos
#define SYS_FS_FORMAT   20  // Formatar sistema de arquivos

// ==========================================================================================================
// Informações do Processo
// ==========================================================================================================

typedef struct {

    uint32_t id;         // PID
    char name[16];       // Nome da Tarefa
    uint32_t state;      // 0=Ready, 1=Running, 2=Blocked
    uint32_t priority;   // Prioridade
    uint32_t sp;         // Stack Pointer atual
    uint64_t wake_time;  // Ciclo de clock para acordar

} task_info_t;

// ==========================================================================================================
//  API DO USUÁRIO (User-Mode Wrappers)
// ==========================================================================================================
//
//    POR QUE USAR 'ECALL' SE SÓ TEMOS MACHINE MODE?
//  
//    Atualmente, nosso processador roda tudo (Kernel e Tarefas) no modo de
//    privilégio máximo (Machine Mode). Não há proteção de hardware impedindo
//    que a tarefa acesse a memória do kernel.
//
//    Ainda assim, usamos a instrução 'ecall' para criar uma FRONTEIRA LÓGICA:
//
//    1. Organização: As tarefas não precisam saber ONDE estão as funções do
//       Kernel ou COMO os drivers funcionam. Elas apenas pedem um serviço.
//
//    2. Padronização: Este mecanismo (colocar ID em a7 e chamar ecall) é o
//       padrão da arquitetura RISC-V (SBI/Linux). Se no futuro for adicionado
//       User Mode (U-Mode) ao hardware, este código continuará funcionando
//       sem alterações.
//
//    COMO FUNCIONA:
//    1. A tarefa coloca o ID da syscall em 'a7' e argumentos em 'a0-a5'.
//    2. A instrução 'ecall' causa uma EXCEÇÃO SÍNCRONA.
//    3. O hardware salva o PC atual em 'mepc' e pula para o 'trap_handler'.
//    4. O Kernel atende o pedido e retorna usando 'mret'.
//
// ==========================================================================================================

/**
 * @brief Cede a vez voluntariamente para a próxima tarefa (Yield).
 * @note  Útil quando a tarefa sabe que vai ficar esperando algo e quer ser "educada",
 *        liberando a CPU para outras tarefas antes do seu tempo acabar (Time Slice).
 */
static inline void sys_yield(void) {

    // Sintaxe geral do GCC Inline Assembly:
    //
    // asm ( "instrucoes"
    //       : outputs   // operandos de saída: valores produzidos pelo asm
    //       : inputs    // operandos de entrada: valores consumidos pelo asm
    //       : clobbers  // registradores/estado que o asm destrói/modifica
    // );
    //
    // O compilador usa essas listas para integrar o asm ao grafo de dependências,
    // garantindo alocação correta de registradores, ordem de execução e
    // preservação de valores vivos.

    // Parâmetros ---------------------------------------------------------------------------
    //
    // -> "instrucoes"
    //   String com assembly literal. Pode usar placeholders (%0, %1, …)
    //   que referenciam outputs/inputs na ordem declarada.
    //
    // -> outputs
    //   Variáveis escritas pelo asm. Informam ao compilador que o valor
    //   anterior é inválido após a execução.
    //
    // -> inputs
    //   Valores lidos pelo asm. Criam dependências para evitar reordenação.
    //
    // -> clobbers
    //   Lista de registradores ou estados ("memory", "cc", etc.) que o asm
    //   altera implicitamente. Evita que o compilador assuma que continuam intactos.
    //
    // --------------------------------------------------------------------------------------

    // Em suma: "Compilador, vou rodar um bloco mágico. Aqui está o que ele lê,
    // escreve e destrói. Organize o resto do programa em volta disso."

    asm volatile (
        "li a7, %0\n"       // Carrega o número SYS_YIELD no registrador a7
        "ecall"             // Dispara a exceção (pula para o Kernel)
        : 
        : "i"(SYS_YIELD)    // Input: Constante imediata (%0)
        : "a7", "memory"    // Clobbers: Avisa o compilador que sujamos a7 e a memória
    );
    
}

/**
 * @brief Escreve um caractere no terminal (UART).
 * @param c Caractere a ser escrito.
 */
static inline void sys_write_char(char c) {
    asm volatile (
        "mv a0, %0\n"       // Move o caractere 'c' (C variable) para o registrador a0
        "li a7, %1\n"       // Move o ID SYS_WRITE para o registrador a7
        "ecall"             // Chama o Kernel!
        : 
        : "r"(c),           // Input %0: variável 'c' em qualquer registrador livre
          "i"(SYS_WRITE)    // Input %1: constante numérica
        : "a0", "a7",       // Clobbers: a0 e a7 foram modificados manualmente
          "memory"
    );
}

/**
 * @brief Função utilitária para escrever strings completas.
 * @note  Esta função roda no espaço do usuário e chama sys_write_char repetidamente.
 */
static inline void sys_puts(const char* s) {
    while (*s) {
        sys_write_char(*s++);
    }
}

/**
 * @brief Bloqueia a tarefa atual por N milissegundos.
 * * A tarefa sai do estado READY/RUNNING e vai para BLOCKED.
 * O escalonador não a escolherá novamente até que o tempo passe.
 * * @param ms Tempo em milissegundos.
 */
static inline void sys_sleep(uint32_t ms) {
    asm volatile (
        "mv a0, %0\n"       // Coloca os milissegundos em a0 (Argumento 1)
        "li a7, %1\n"       // Coloca o ID SYS_SLEEP em a7
        "ecall"             // Trap!
        : 
        : "r"(ms),          // Input %0
          "i"(SYS_SLEEP)    // Input %1
        : "a0", "a7", "memory"
    );
}

// Tenta trancar o mutex.
// Retorna 1 se conseguiu a chave.
// Retorna 0 se já estava trancado (falhou).
static inline int sys_mutex_lock(mutex_t *m) {
    int ret;
    asm volatile (
        "mv a0, %1\n"       // Arg0: Endereço do mutex
        "li a7, %2\n"       // ID: SYS_LOCK
        "ecall\n"           // Chama o Kernel
        "mv %0, a0"         // Pega o retorno (a0) e põe em 'ret'
        : "=r"(ret) 
        : "r"(m), "i"(SYS_LOCK) 
        : "a0", "a7", "memory"
    );
    return ret;
}

// Destranca o mutex (libera para outros).
static inline void sys_mutex_unlock(mutex_t *m) {
    asm volatile (
        "mv a0, %0\n"       // Arg0: Endereço do mutex
        "li a7, %1\n"       // ID: SYS_UNLOCK
        "ecall"
        : : "r"(m), "i"(SYS_UNLOCK) 
        : "a0", "a7", "memory"
    );
}

// Retorna o número de tarefas encontradas
static inline int sys_get_tasks(task_info_t *buffer, int max_count) {
    int ret;
    asm volatile (
        "mv a0, %1\n"       // Arg0: Ponteiro para o buffer
        "mv a1, %2\n"       // Arg1: Tamanho máximo do buffer
        "li a7, %3\n"       // ID: SYS_GET_TASKS
        "ecall\n"
        "mv %0, a0"         // Retorno: Quantidade de tarefas copiadas
        : "=r"(ret) 
        : "r"(buffer), "r"(max_count), "i"(SYS_GET_TASKS) 
        : "a0", "a1", "a7", "memory"
    );
    return ret;
}

// Lê 32 bits de um endereço físico
static inline uint32_t sys_peek(uint32_t addr) {
    uint32_t val;
    asm volatile (
        "mv a0, %1\n"
        "li a7, %2\n"
        "ecall\n"
        "mv %0, a0"
        : "=r"(val) 
        : "r"(addr), "i"(SYS_PEEK) 
        : "a0", "a7", "memory"
    );
    return val;
}

// Escreve 32 bits em um endereço físico
static inline void sys_poke(uint32_t addr, uint32_t val) {
    asm volatile (
        "mv a0, %0\n"
        "mv a1, %1\n"
        "li a7, %2\n"
        "ecall"
        : 
        : "r"(addr), "r"(val), "i"(SYS_POKE) 
        : "a0", "a1", "a7", "memory"
    );
}

// Dump do Heap
static inline void sys_heap_info(void) {
    asm volatile ("li a7, %0; ecall" :: "i"(SYS_HEAP_INFO) : "a7", "memory");
}

// Pausa processo pelo PID
static inline int sys_suspend(uint32_t pid) {
    int ret; asm volatile ("mv a0, %1; li a7, %2; ecall; mv %0, a0" : "=r"(ret) : "r"(pid), "i"(SYS_SUSPEND) : "a0", "a7"); return ret;
}

// Retoma processo pelo PID
static inline int sys_resume(uint32_t pid) {
    int ret; asm volatile ("mv a0, %1; li a7, %2; ecall; mv %0, a0" : "=r"(ret) : "r"(pid), "i"(SYS_RESUME) : "a0", "a7"); return ret;
}

// Aloca memória no Heap do Kernel
static inline void* sys_malloc(uint32_t size) {
    void* ret; asm volatile ("mv a0, %1; li a7, %2; ecall; mv %0, a0" : "=r"(ret) : "r"(size), "i"(SYS_MALLOC) : "a0", "a7"); return ret;
}


// Libera memória alocada no Heap do Kernel
static inline uint8_t sys_free(void* ptr) {
    int res;
    asm volatile (
        "mv a0, %1\n"       // Move o ponteiro para a0 (Argumento)
        "li a7, %2\n"       // Carrega o ID da Syscall em a7
        "ecall\n"           // Chama o Kernel
        "mv %0, a0\n"       // <--- CAPTURA: Move o retorno de a0 para a variável 'res'
        : "=r"(res)         // Saída: 'res' está associado a %0
        : "r"(ptr), "i"(SYS_FREE) // Entradas: %1 e %2
        : "a0", "a7", "memory"    // Clobbers
    );
    return res;
}

// Desfragmenta o heap do Kernel
static inline void sys_defrag(void) {
    asm volatile (
        "li a7, %0\n"       // Carrega ID SYS_DEFRAG (40) em a7
        "ecall"             // Chama o Kernel
        : 
        : "i"(SYS_DEFRAG) 
        : "a0", "a7", "memory"
    );
}

#endif /* SYSCALL_H */