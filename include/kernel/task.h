#ifndef TASK_H
#define TASK_H

#include <stdint.h>

// ============================================================================
//  CONFIGURAÇÕES DO SISTEMA
// ============================================================================

// Tamanho da pilha de cada tarefa (1KB).
// Cada função chamada, variável local e registrador salvo consome espaço aqui.
// Se a pilha estourar (Stack Overflow), a tarefa corrompe a memória vizinha!
#define STACK_SIZE 1024

// Número máximo de tarefas simultâneas.
// Usamos alocação estática (array fixo) para simplificar e evitar fragmentação.
#define MAX_TASKS  4

// ============================================================================
//  ESTADOS DA TAREFA
// ============================================================================

typedef enum {
    TASK_READY,      // Pronta para rodar (está na fila aguardando a CPU)
    TASK_RUNNING,    // Está rodando neste exato momento (posse da CPU)
    TASK_BLOCKED,    // Dormindo (Sleep) ou esperando recurso (Mutex/IO)
    TASK_SUSPENDED,  // Pausado, mas existe
    TASK_TERMINATED  // Morto, pronto para limpeza
} task_state_t;

// ============================================================================
//  CONTEXTO DE HARDWARE (CPU SNAPSHOT)
// ============================================================================
//
//  IMPORTANTE: O layout desta estrutura deve coincidir EXATAMENTE com a ordem
//  de push/pop no arquivo 'trap.s'.
//
// ============================================================================

// Total: 32 registradores (32 * 4 bytes = 128 bytes)

typedef struct {

    // --- Registradores Gerais (x1 a x31) ---

    uint32_t ra;   // x1  (Return Address): Para onde a função volta
    uint32_t sp;   // x2  (Stack Pointer): O topo da pilha (salvo separadamente também)
    uint32_t gp;   // x3  (Global Pointer): Acesso a variáveis globais
    uint32_t tp;   // x4  (Thread Pointer): Dados locais da thread
    uint32_t t0;   // x5  (Temporário)
    uint32_t t1;   // x6  (Temporário)
    uint32_t t2;   // x7  (Temporário)
    uint32_t s0;   // x8  (Saved/Frame Pointer): Base da pilha da função
    uint32_t s1;   // x9  (Saved Register)
    uint32_t a0;   // x10 (Argumento 0 / Retorno de função)
    uint32_t a1;   // x11 (Argumento 1 / Retorno de função)
    uint32_t a2;   // x12 (Argumento 2)
    uint32_t a3;   // x13 (Argumento 3)
    uint32_t a4;   // x14 (Argumento 4)
    uint32_t a5;   // x15 (Argumento 5)
    uint32_t a6;   // x16 (Argumento 6)
    uint32_t a7;   // x17 (Argumento 7 / Número da Syscall)
    uint32_t s2;   // x18 (Saved Register)
    uint32_t s3;   // x19
    uint32_t s4;   // x20
    uint32_t s5;   // x21
    uint32_t s6;   // x22
    uint32_t s7;   // x23
    uint32_t s8;   // x24
    uint32_t s9;   // x25
    uint32_t s10;  // x26
    uint32_t s11;  // x27
    uint32_t t3;   // x28 (Temporário)
    uint32_t t4;   // x29
    uint32_t t5;   // x30
    uint32_t t6;   // x31

    // --- Registradores de Controle (CSRs) ---

    uint32_t mepc; // (Machine Exception PC): Endereço exato onde o código parou.
                   // É para cá que o 'mret' vai pular ao acordar a tarefa.

} context_t;

// ============================================================================
//  TASK CONTROL BLOCK (TCB)
// ============================================================================
//
//  Esta é a estrutura que o Kernel usa para gerenciar o processo.
//  A CPU não sabe o que é isso; para a CPU, só existe o contexto acima.
//
// ============================================================================

typedef struct task_t {

    // Identificação
    uint32_t      tid;                 // ID único da tarefa (0, 1, 2...)
    char          name[16];            // Nome legível (ex: "Shell", "Sensor")

    // Controle de Execução
    task_state_t  state;               // Estado atual (READY, BLOCKED...)
    uint32_t      priority;            // 0 (Idle) a N (Máxima)
    
    // Stack Pointer
    // Este campo guarda o endereço de memória onde o 'context_t' desta tarefa
    // foi salvo pela última vez. O 'scheduler' entrega este endereço para o 'trap.s'.
    uint32_t      sp;            
    
    // Gestão de Tempo (Syscall Sleep)
    // Armazena o valor absoluto do contador de ciclos (mtime) quando a tarefa deve acordar.
    uint64_t      wake_time;     
    
    // A Memória da Tarefa (Pilha)
    // context_t é salvo dentro deste array.
    uint8_t       stack[STACK_SIZE]; 

} task_t;

// ============================================================================
//  VARIÁVEIS GLOBAIS DO KERNEL
// ============================================================================

// O código Assembly (trap.s) precisa acessar estas variáveis para saber
// qual pilha salvar e qual pilha carregar.

extern task_t *current_task; // Quem estava rodando antes do Trap
extern task_t *next_task;    // Quem vai rodar depois do Trap

// ============================================================================
//  API DO ESCALONADOR
// ============================================================================

// Inicializa as estruturas internas
void scheduler_init(void);

// Coloca a tarefa atual para dormir por N milissegundos
void scheduler_sleep(uint32_t ms);

// Cria uma nova tarefa e a coloca na fila READY
// Retorna o TID ou -1 em caso de erro.
int task_create(void (*function)(void), const char* name, uint32_t priority);

// O algoritmo de decisão: escolhe quem é o próximo 'next_task'
void schedule(void);

#endif