// ======================================================================================
//  ARQUIVO   : scheduler.c
//  DESCRIÇÃO : Escalonador/Agendador do Sistema Operacional.
// ======================================================================================
//
//  RESPONSABILIDADES:
//  1. Gerenciar a lista de tarefas (Process Control Block / TCB).
//  2. Criar novas tarefas "falsificando" um contexto inicial na pilha.
//  3. Decidir qual tarefa roda a seguir (Escalonamento).
//  4. Gerenciar o bloqueio de tarefas (Sleep) para economizar CPU.
//
// ======================================================================================

#include "../../include/kernel/task.h"
#include "../../include/hal/hal_uart.h"
#include "../../include/hal/hal_timer.h"
#include "../../include/kernel/logger.h"
#include "../../include/sys/syscall.h"
#include <stddef.h>

// ======================================================================================
//  ESTRUTURAS DE DADOS DO KERNEL
// ======================================================================================

// Pool estático de tarefas.
// Em um SO real, usaríamos alocação dinâmica (malloc), mas em sistemas embarcados
// críticos, alocação estática é mais segura e previsível.

static task_t tasks[MAX_TASKS];
static int task_count = 0;

// PONTEIROS GLOBAIS (Acessados pelo Assembly 'trap.s')

// current_task: Quem está usando a CPU agora.
// next_task:    Quem o scheduler decidiu que vai usar a CPU a seguir.

task_t *current_task = NULL;
task_t *next_task = NULL;

// Se next_task != current_task, o trap.s realiza a Troca de Contexto.

// ======================================================================================
//  INICIALIZAÇÃO
// ======================================================================================

void scheduler_init(void) {
    task_count = 0;
    current_task = NULL;
    log_sched("Secheduler Initialized!\n\r");
}

// ======================================================================================
//  GESTÃO DE TEMPO (SLEEP)
// ======================================================================================

/**
 * @brief Coloca a tarefa atual para dormir.
 * A tarefa sai da fila de prontas e entra em estado BLOCKED.
 * O processador não desperdiçará ciclos com ela até o tempo acabar.
 */
void scheduler_sleep(uint32_t ms) {

    if (current_task == NULL) return;

    // 1. Converter milissegundos em Ciclos de Clock
    // Precisamos saber a frequência do hardware (QEMU=10MHz, FPGA=100MHz)
    uint32_t freq = hal_timer_get_freq();
    
    // Fórmula: Ciclos = MS * (Ciclos_por_Segundo / 1000)
    uint64_t cycles_to_wait = (uint64_t)ms * (freq / 1000);
    
    // Define o "Despertador": Hora atual + Espera
    current_task->wake_time = hal_timer_get_cycles() + cycles_to_wait;
    
    // 2. MUDANÇA DE ESTADO: RUNNING -> BLOCKED
    // O scheduler (função schedule abaixo) vai ignorar esta tarefa nas próximas rodadas.
    current_task->state = TASK_BLOCKED;
    
    // 3. Ceder a vez IMEDIATAMENTE.
    // Chamamos o scheduler para trocar para outra tarefa útil.
    // Sem aguardar pelo próximo tick do timer.
    schedule();

}

// ======================================================================================
//  CRIAÇÃO DE TAREFAS (A ARTE DA FALSIFICAÇÃO)
// ======================================================================================
/**
 * @brief Cria uma nova tarefa pronta para execução.
 * * O processador RISC-V não sabe o que é uma "tarefa". Ele apenas segue o fluxo
 * de instruções e usa a pilha (SP). Para criar uma tarefa, precisamos "falsificar"
 * uma pilha que pareça que a tarefa foi interrompida anteriormente.
 * * Quando o 'trap.s' fizer a troca para esta tarefa pela primeira vez, ele vai
 * "restaurar" esse contexto falso, carregando o endereço da função no PC.
 */
int task_create(void (*function)(void), const char* name, uint32_t priority) {

    // 0. Verifica a quantidade de TASKS já existentes
    if (task_count >= MAX_TASKS) {
        log_sched("Error: Max tasks reached.\n\r");
        return -1;
    }

    // 1. Aloca um TCB (Task Control Block) livre
    task_t *t = &tasks[task_count];
    t->tid = task_count;
    t->state = TASK_READY;
    t->priority = priority;
    
    // Copia o nome (segurança simples)
    for(int i=0; i<15 && name[i]; i++) t->name[i] = name[i];
    
    // --- STACK FORGING (Montagem da Pilha Falsa) ---
    
    // A. Aponta para o TOPO da pilha (Stacks crescem para baixo na memória)
    uint32_t sp = (uint32_t)&t->stack[STACK_SIZE];
    
    // B. Reserva espaço para o Contexto (registradores que o trap.s salva/restaura)
    sp -= sizeof(context_t);
    
    // C. Mapeia a estrutura context_t nesse endereço da memória
    context_t *ctx = (context_t *)sp;
    
    // D. Limpa os registradores (Zera tudo para evitar lixo)
    for (int i=0; i<32; i++) ((uint32_t*)ctx)[i] = 0;
    
    // E. Configura os Registradores Críticos:
    
    // RA (Return Address): Para onde a função volta se der 'return'?
    // Como nossas tarefas são loops infinitos while(1), isso teoricamente não ocorre.
    // Mas apontamos para a própria função por segurança.
    ctx->ra = (uint32_t)function;
    
    // MEPC (Machine Exception PC): Onde a execução começa quando dermos 'mret'?
    // Apontamos para o início da função da tarefa.
    ctx->mepc = (uint32_t)function;

    // GP (Global Pointer): Código C compilado usa o registrador 'gp' 
    // para acessar variáveis globais (como 'tasks', 'current_task', etc.) de forma eficiente.
    // Se não copiarmos o 'gp' atual do Kernel para a nova tarefa, ela vai crashar
    // ao tentar ler qualquer variável global.
    uint32_t global_pointer;
    asm volatile("mv %0, gp" : "=r"(global_pointer));
    ctx->gp = global_pointer;
    
    // F. Salva o ponteiro da nova pilha no TCB
    // O trap.s vai carregar este valor no registrador SP da CPU.
    t->sp = sp;
    
    log_sched("Created task: ");
    hal_uart_puts(name);
    hal_uart_puts("\n\r");

    task_count++;
    return t->tid;

}

// ======================================================================================
// Informações das Tasks no Sistema
// ======================================================================================

// Retorna quantos processos foram copiados
int scheduler_get_tasks_info(task_info_t *user_buffer, int max_count) {
    int count = 0;
    
    for (int i = 0; i < task_count && count < max_count; i++) {
        // Copia dados do TCB interno para a struct pública
        user_buffer[i].id        = tasks[i].tid;
        user_buffer[i].state     = tasks[i].state;
        user_buffer[i].priority  = tasks[i].priority;
        user_buffer[i].sp        = tasks[i].sp;
        user_buffer[i].wake_time = tasks[i].wake_time;
        
        // Copia o nome (strcpy manual seguro)
        for (int j = 0; j < 16; j++) {
            user_buffer[i].name[j] = tasks[i].name[j];
        }
        
        count++;
    }
    return count;
}

// ======================================================================================
//  ALGORITMO DE ESCALONAMENTO
// ======================================================================================

/**
 * @brief Escolhe a próxima tarefa a ser executada.
 * Chamado periodicamente pelo Timer (Tick) ou voluntariamente (Yield/Sleep).
 * O escalonador mantém o equilíbrio da Força entre as tarefas.
 */
void schedule(void) {
    if (task_count == 0) return;

    // ----------------------------------------------------------------------------------
    // FASE 1: O DESPERTAR DA FORÇA (Unblock)
    // ----------------------------------------------------------------------------------

    // Verifica todas as tarefas dormindo. 
    // Se o tempo passou, elas despertam para o mundo real.

    uint64_t now = hal_timer_get_cycles();
    
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_BLOCKED) {
            if (now >= tasks[i].wake_time) {
                tasks[i].state = TASK_READY; // Volta para a fila de prontas
            }
        }
    }

    // ----------------------------------------------------------------------------------
    // FASE 2: O ESCOLHIDO (Round Robin com Prioridade)
    // ----------------------------------------------------------------------------------

    // Encontrar uma tarefa READY com a maior prioridade possível.
    // Se ninguém quiser rodar, cai na Idle Task.
    
    task_t *best_task = NULL;
    
    // Começamos a busca a partir do próximo ID para garantir justiça (Round Robin).
    // Se sempre começássemos do 0, a tarefa 0 teria vantagem injusta.

    int start_id = (current_task) ? (current_task->tid + 1) % task_count : 0;
    int i = start_id;
    
    // Loop circular

    do {

        // Critério 1: Tarefa deve estar Pronta ou Rodando

        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING) {
            
            // Critério 2: Prioridade > 0 (Ignora a Idle Task por enquanto)
            // Queremos dar chance para as tarefas úteis primeiro.

            if (tasks[i].priority > 0) {
                best_task = &tasks[i];
                break; // Achamos! Para a busca.
            }

        }
        
        // Avança circularmente (0, 1, 2, 0, 1...)
        i = (i + 1) % task_count;
        
    } while (i != start_id);

    // ----------------------------------------------------------------------------------
    // FASE 3: O GUARDIÃO 
    // ----------------------------------------------------------------------------------

    // Se best_task ainda é NULL, significa que todas as tarefas úteis estão
    // dormindo (BLOCKED). Precisamos rodar a Idle Task (Prioridade 0).
    
    if (best_task == NULL) {
        for (int k=0; k<task_count; k++) {
            if (tasks[k].priority == 0) { // Identifica a Idle pela prioridade
                best_task = &tasks[k];
                break;
            }
        }
    }

    // ----------------------------------------------------------------------------------
    // FASE 4: TRANSIÇÃO DE CONTEXTO LÓGICA
    // ----------------------------------------------------------------------------------

    // Define a próxima tarefa e ajusta estados:
    //  - READY  -> RUNNING
    //  - RUNNING (antiga) -> READY se houve preempção

    if (best_task != NULL) {

        next_task = best_task;
        
        // Se ela estava apenas PRONTA, agora ela é a DONA DA CPU.
        if (next_task->state == TASK_READY) {
            next_task->state = TASK_RUNNING;
        }
        
        // O current_task deixa de ser RUNNING?
        // Sim, se ele não bloqueou, ele volta para READY (Preempção).
        if (current_task && current_task->state == TASK_RUNNING && current_task != next_task) {
             current_task->state = TASK_READY;
        }

    }

}