#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "mutex.h"

// ==========================================================================================================
//  TABELA DE NÚMEROS DE SYSCALL
// ==========================================================================================================

// Estes números são o "código do pedido". O Kernel usa um switch/case no 
// trap_handler para saber qual serviço executar.

#define SYS_YIELD   1   // Ceder a CPU voluntariamente
#define SYS_WRITE   2   // Escrever no console (UART)
#define SYS_SLEEP   3   // Dormir por N milissegundos
#define SYS_LOCK    4   // Tentar pegar a chave
#define SYS_UNLOCK  5   // Devolver a chave

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

#endif /* SYSCALL_H */