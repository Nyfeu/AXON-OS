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

    // Se não há tarefas, nada a fazer
    if (task_count == 0) return;

    // ======================================================================================
    // FASE 1: O GRANDE DESPERTAR 
    // ======================================================================================

    // Tipo aquele momento em que você acorda e pensa "será que durmi o suficiente?"
    // Aqui a gente acorda as tasks que pediram sleep() e seu tempo expirou.
    
    uint64_t now = hal_timer_get_cycles();
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == TASK_BLOCKED) {
            // "Ei, acordaaaa! Seu timer zerou!"
            if (now >= tasks[i].wake_time) {
                tasks[i].state = TASK_READY; 
            }
        }
    }

    // ======================================================================================
    // FASE 2: A LUTA PELA CPU (ESCALONAMENTO)
    // ======================================================================================

    // Algoritmo: Round-Robin baseado em Prioridade (CLINT faz a preempção via Timer)

    // Apenas UMA task pode rodar por vez.
    // Regra 1: Ganha quem tiver a MAIOR prioridade.
    // Regra 2: Em caso de empate, quem chegar primeiro na busca circular (Round Robin).

    task_t *best_task = NULL;
    int max_prio_found = -1;  // Variável que registra a maior prioridade encontrada

    // Ponto de partida: Não começamos sempre da task 0 (seria injusto).
    // Começamos do ID seguinte à atual, garantindo uma certa justiça cíclica.
    int start_id = (current_task) ? (current_task->tid + 1) % task_count : 0;

    // Rodamos exatamente task_count vezes de forma circular
    for (int j = 0; j < task_count; j++) {
        int i = (start_id + j) % task_count;

        // Filtrando os "aptos": Só tasks READY ou RUNNING entram na luta
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING) {
            
            // - Se best_task é NULL: Pegamos o primeiro candidato que achar
            // - Se encontramos alguém com prioridade ESTRITAMENTE MAIOR: Trocamos de candidato
            // - Se a prioridade for IGUAL: Mantemos o primeiro que achou (ele tem direito!)
            
            if (best_task == NULL || tasks[i].priority > max_prio_found) {
                best_task = &tasks[i];
                max_prio_found = tasks[i].priority;
            }
        }
    }

    // Se ninguém quer rodar,
    // Pegamos a Idle Task (confiável e prioridade 0).
    if (best_task == NULL) {
        for (int k = 0; k < task_count; k++) {
            if (tasks[k].priority == 0) {
                best_task = &tasks[k];
                break;
            }
        }
    }

    // ======================================================================================
    // FASE 3: O SALTO - Troca de Contexto
    // ======================================================================================

    // Se chegou até aqui e encontrou alguém, é hora de fazer o "context switch".
    
    if (best_task != NULL) {
        next_task = best_task;
        
        // Se a próxima task estava apenas PRONTA, agora vira RUNNING
        if (next_task->state == TASK_READY) {
            next_task->state = TASK_RUNNING;
        }
        
        // Se a task ATUAL está rodando e vai ser desbancada, volta para PRONTA
        if (current_task && current_task->state == TASK_RUNNING && current_task != next_task) {
             current_task->state = TASK_READY;
        }

    }
    
}

// ======================================================================================
// FUNÇÕES DE CONTROLE DE TAREFAS
// ======================================================================================

// Pausa a tarefa com o PID especificado (SUSPENDED)
int scheduler_suspend(uint32_t pid) {
    if (tasks[pid].priority == 0) return -1; // Não pode pausar a Idle
    tasks[pid].state = TASK_SUSPENDED;
    if (current_task->tid == pid) schedule(); // Se pausou a si mesmo, cede a vez
    return 0;
}

// Continua a tarefa com o PID especificado (SUSPENDED -> READY)
int scheduler_resume(uint32_t pid) {
    if (pid >= task_count) return -1;
    if (tasks[pid].state == TASK_SUSPENDED) {
        tasks[pid].state = TASK_READY;
    }
    return 0;
}
