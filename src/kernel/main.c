// ======================================================================================
//  AXON RTOS - KERNEL MAIN
// ======================================================================================
//
//  FLUXO DE EXECUÇÃO:
//  1. Bootloader (start.s) configura a pilha e chama kernel_main().
//  2. kernel_main() inicializa drivers e cria as tarefas.
//  3. kernel_main() chama schedule() e "morre" (o controle passa para as tarefas).
//  4. O sistema vive de Interrupções (Timer) e Exceções (Syscalls).
//
// ======================================================================================

#include "../../include/hal_uart.h"
#include "../../include/hal_timer.h"
#include "../../include/hal_irq.h"
#include "../../include/hal_plic.h" 
#include "../../include/task.h"
#include "../../include/syscall.h"
#include "../../include/logger.h" 
#include "../../include/mutex.h"
#include "../../include/apps.h"
#include "../../include/mm.h"

// ======================================================================================
//  PROTÓTIPOS DE FUNÇÕES
// ======================================================================================

int scheduler_get_tasks_info(void *user_buffer, int max_count);

// ======================================================================================
//  CONFIGURAÇÕES GLOBAIS
// ======================================================================================

// Intervalo do Timer (Heartbeat do SO)
// Define quanto tempo cada tarefa roda antes de sofrer preempção.
// 1000000 ciclos @ 10MHz = 100ms.
#define TICK_DELTA_CYCLES 1000000 

// Definido no trap.s
extern void trap_entry();

// Símbolos do Linker
extern void _start(void);
extern void _end(void);

// Task atual rodando
extern task_t *current_task;

// ======================================================================================
//  HELPERS
// ======================================================================================

void print_dec(uint32_t n) {
    if (n == 0) {
        hal_uart_putc('0');
        return;
    }

    char buf[12];
    int i = 0;

    // Extrai dígitos de trás para frente
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }

    // Imprime na ordem correta
    while (i > 0) {
        hal_uart_putc(buf[--i]);
    }
}

// ======================================================================================
// Tarefa IDLE 
// ======================================================================================

// O scheduler escolhe esta tarefa quando ninguém mais quer rodar.
// Sua função é economizar energia.

void task_idle(void) {
    while (1) {
        
        // WFI (Wait For Interrupt): Instrução RISC-V que suspende o clock
        // da CPU até que uma interrupção (ex: Timer) ocorra.
        hal_timer_idle();

    }
}

// ======================================================================================
//  TRATADOR CENTRAL DE EVENTOS (TRAP HANDLER)
// ======================================================================================
//  Esta função é chamada pelo Assembly (trap.s) sempre que a CPU para.
//  Pode ser por dois motivos:
//  1. INTERRUPÇÃO (Assíncrona): O hardware quer atenção (Timer, Rede, etc).
//  2. EXCEÇÃO (Síncrona): O software fez algo (Syscall, Erro, Divisão por 0).
//
//  Parâmetros vindos do Assembly:
//  - mcause: O motivo da parada.
//  - mepc:   Onde o código parou.
//  - ctx:    Ponteiro para os registradores salvos na pilha da tarefa.
// ======================================================================================

void trap_handler(unsigned int mcause, unsigned int mepc, uint32_t *ctx) {

    // Bit mais significativo define se é Interrupção (1) ou Exceção (0)
    int is_interrupt = (mcause >> 31);
    int cause_code = mcause & 0x7FFFFFFF; // Remove o bit de sinal

    if (is_interrupt) {
        // ==============================================================================
        //  TRATAMENTO DE INTERRUPÇÕES (HARDWARE)
        // ==============================================================================
        switch (cause_code) {
            case 7: // Machine Timer Interrupt
                // 1. Re-armar o alarme para o futuro
                hal_timer_set_irq_delta(TICK_DELTA_CYCLES);
                
                // 2. O tempo da tarefa atual acabou (Time Slice).
                // Chamamos o chefe (Scheduler) para escolher a próxima.
                schedule();
                break;
            
            case 11: // Machine External Interrupt (PLIC)
                {
                    // Usado para UART RX, Botões, etc.
                    // Agora usamos o Dispatcher para PLIC
                    uint32_t source = hal_plic_claim();
                    
                    if (source) {

                        // O Dispatcher olha na tabela e chama a função registrada
                        irq_dispatch(source);
                        
                        // Avisa o hardware que terminamos
                        hal_plic_complete(source);

                    }

                }
                break;
        }
    } else {
        // ==============================================================================
        //  TRATAMENTO DE EXCEÇÕES (SOFTWARE)
        // ==============================================================================
        
        // Causa 11 = Environment Call (ECALL) vinda do Machine Mode.
        // É assim que as tarefas "chamam" o Kernel.
        if (cause_code == 11) {
            
            // Lendo os argumentos da Syscall direto da pilha salva (ABI RISC-V)
            uint32_t syscall_num = ctx[16]; // a7: ID da Syscall
            uint32_t arg0        = ctx[9];  // a0: Primeiro argumento

            switch (syscall_num) {
                case SYS_YIELD:
                    // Tarefa diz: "Pode passar minha vez"
                    schedule();
                    break;

                case SYS_WRITE:
                    // Tarefa diz: "Escreve este char na tela"
                    hal_uart_putc((char)arg0);
                    break;

                case SYS_SLEEP:
                    // Tarefa diz: "Me acorde daqui a X ms"
                    scheduler_sleep(arg0);
                    break;

                case SYS_LOCK: {
                        // O argumento a0 é o ponteiro para o mutex
                        mutex_t *m = (mutex_t *)arg0;
                        
                        // Lógica Atômica (Kernel Mode):
                        if (m->locked == 0) {
                            // Sucesso! A porta estava aberta.
                            m->locked = 1;
                            m->owner_tid = current_task->tid;
                            ctx[9] = 1; // Retorna 1 em a0 (ctx[9])
                        } else {
                            // Falha! Alguém já trancou.
                            ctx[9] = 0; // Retorna 0 em a0
                        }
                    }
                    break;

                case SYS_UNLOCK: {
                        mutex_t *m = (mutex_t *)arg0;
                        // Segurança: Só o dono pode destrancar!
                        if (m->locked && m->owner_tid == current_task->tid) {
                            m->locked = 0;
                            m->owner_tid = 0;
                            // Assim que destrancamos, outra tarefa pode tentar pegar.
                            // O scheduler vai decidir quem roda a seguir.
                        }
                    }
                    break;

                case SYS_GET_TASKS: {
                        // a0 = buffer, a1 = max_count
                        void *buf = (void *)arg0;
                        int max   = (int)ctx[10]; // a1 está no índice 10 do contexto (ver task.h/trap.s)
                        
                        // Chama a função do scheduler e retorna a contagem em a0
                        ctx[9] = scheduler_get_tasks_info(buf, max); 
                    }
                    break;
                    
                default:
                    hal_uart_puts("[KERNEL] Syscall desconhecida.\n\r");
                    break;
            }

            // [CRÍTICO] Avançar o PC (Program Counter)
            // A instrução 'ecall' tem 4 bytes. Se não somarmos 4 ao endereço
            // de retorno (mepc), quando dermos 'mret', a CPU vai executar
            // a MESMA instrução 'ecall' novamente, criando um loop infinito.
            // ctx[31] mapeia para o campo 'mepc' na struct context_t.
            ctx[31] += 4;

        } else {

            // Se chegamos aqui, foi um erro grave (Crash, Instrução Ilegal, etc)
            hal_uart_puts(ANSI_RED "\n\r[CRIT] EXCEPTION DETECTED!\n\r");
            hal_uart_puts("   > MCAUSE: "); print_hex(mcause); hal_uart_puts("\n\r");
            hal_uart_puts("   > MEPC:   "); print_hex(mepc);   hal_uart_puts("\n\r");
            hal_uart_puts("System Halted.\n\n" ANSI_RESET);
            
            hal_uart_puts(ANSI_YELLOW "System will reboot in 3 seconds...\n\r" ANSI_RESET);

            // Não podemos usar sys_sleep (precisa de scheduler).
            // Usamos o Hardware Timer (independente de interrupções)
            hal_timer_delay_ms(3000);

            // Mensagem de aviso
            hal_uart_puts("Rebooting now!\n\r");
            _start();

        }
    }
}

// ======================================================================================
//  BOOTSTRAP DO SISTEMA (MAIN)
// ======================================================================================

void kernel_main() {

    // ----------------------------------------------------------------------------------
    // FASE 1: Inicialização de Hardware (Drivers)
    // ----------------------------------------------------------------------------------
    
    hal_uart_init();                           // Inicializa UART
    hal_uart_puts("\033[2J\033[H\033[2;0H");   // Limpar tela

    // Mensagem com ASCII ART BANNER
    hal_uart_puts(ANSI_CYAN ANSI_BOLD);
    hal_uart_puts("\n\r");
    hal_uart_puts("   █████╗ ██╗  ██╗ ██████╗ ███╗   ██╗       ██████╗ ███████╗\n\r");
    hal_uart_puts("  ██╔══██╗╚██╗██╔╝██╔═══██╗████╗  ██║      ██╔═══██╗██╔════╝\n\r");
    hal_uart_puts("  ███████║ ╚███╔╝ ██║   ██║██╔██╗ ██║█████╗██║   ██║███████╗\n\r");
    hal_uart_puts("  ██╔══██║ ██╔██╗ ██║   ██║██║╚██╗██║╚════╝██║   ██║╚════██║\n\r");
    hal_uart_puts("  ██║  ██║██╔╝ ██╗╚██████╔╝██║ ╚████║      ╚██████╔╝███████║\n\r");
    hal_uart_puts("  ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝       ╚═════╝ ╚══════╝\n\r");   
    hal_uart_puts("\n\r");
    hal_uart_puts(ANSI_RESET);
    hal_uart_puts("     :: AXON RTOS :: (v0.1.0-alpha) \n\r");
    hal_uart_puts("     :: Build: RISC-V 32-bit (RV32I_Zicsr) \n\r");
    hal_uart_puts("\n\r");

    // Mensagem de aviso da inicilização do processo de boot
    log_info("Boot sequence initiated...");
    
    // Diagnóstico de Memória
    uint32_t kernel_start = (uint32_t)_start;
    uint32_t kernel_end   = (uint32_t)_end;
    uint32_t kernel_size  = kernel_end - kernel_start;

    hal_uart_puts(ANSI_CYAN "[ MEM   ] Kernel Memory Usage:\n\r");
    hal_uart_puts("\n  > Start: "); print_hex(kernel_start); hal_uart_puts("\n\r");
    hal_uart_puts("  > End:   "); print_hex(kernel_end);   hal_uart_puts("\n\r");
    hal_uart_puts("  > Size:  "); 
    print_dec(kernel_size); 
    hal_uart_puts(" bytes (");
    print_dec(kernel_size / 1024);
    hal_uart_puts(" KB)\n\n\r" ANSI_RESET);

    // Inicialização da HEAP

    // Definição do tamanho da RAM Total 
    uint32_t ram_total = 64 * 1024;

    // O Heap começa no fim do Kernel (_end)
    // Reservamos 4KB (4096) no final da RAM para a Pilha de Boot (Stack de segurança)
    uint32_t heap_size = ram_total - kernel_size - 4096;

    hal_uart_puts(ANSI_CYAN "[ MEM   ] Initializing Heap Manager...\n\n\r");
    kmalloc_init((void*)_end, heap_size);
    
    // Verifica quanto sobrou
    uint32_t free_ram = kget_free_memory();
    hal_uart_puts("  > Available Heap: "); 
    print_dec(free_ram); 
    hal_uart_puts(" bytes.\n\r" ANSI_RESET);
    hal_uart_putc('\n');

    // ----------------------------------------------------------------------------------
    // FASE 2: Configuração de Interrupções
    // ----------------------------------------------------------------------------------
    
    log_info("Configuring Trap Vector Table...");
    
    // Diz à CPU para pular para 'trap_entry' (no assembly) quando algo acontecer
    hal_irq_set_handler(trap_entry);
    
    // Configura e ativa o Timer do sistema
    log_info("Starting System Timer...");
    hal_timer_set_irq_delta(TICK_DELTA_CYCLES);
    hal_irq_mask_enable(IRQ_M_TIMER);
    
    // Libera as interrupções globais (MIE bit)
    hal_irq_global_enable();
    log_ok("Interrupts Enabled.");

    // Inicializa interrupções de plataforma (PLIC)
    hal_irq_init();  

    // Inicializa MUTEXES
    apps_init();

    // ----------------------------------------------------------------------------------
    // FASE 3: Criação de Processos
    // ----------------------------------------------------------------------------------

    log_info("Initializing Process Scheduler...");
    scheduler_init();
    
    // Cria as tarefas do usuário (Pilha, Contexto, TCB)
    task_create(task_a, "Task A", 1);
    task_create(task_b, "Task B", 1);
    task_create(task_shell, "Shell", 2);
    
    // Cria a tarefa de background (obrigatória para o scheduler não falhar)
    task_create(task_idle, "Idle", 0);
    
    hal_uart_putc('\n');
    hal_uart_puts(ANSI_GREEN ">>> AXON KERNEL IS READY. <<<\n\n\r" ANSI_RESET);
    
    // ----------------------------------------------------------------------------------
    // FASE 4: Agendamento das Tarefas
    // ----------------------------------------------------------------------------------

    // Aqui chamamos o scheduler pela primeira vez.
    // Ele vai carregar o SP (Stack Pointer) da Task A.
    // O fluxo deste 'kernel_main' MORRE aqui. Nunca passaremos desta linha.
    // A partir de agora, quem manda são as tarefas e o Trap Handler.

    schedule(); 
    
    // Código inalcançável (Failsafe)
    while (1) asm volatile("nop");

}