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

// ======================================================================================
//  CONFIGURAÇÕES GLOBAIS
// ======================================================================================

// Intervalo do Timer (Heartbeat do SO)
// Define quanto tempo cada tarefa roda antes de sofrer preempção.
// 1000000 ciclos @ 10MHz = 100ms.
#define TICK_DELTA_CYCLES 1000000 

// Definido no trap.s
extern void trap_entry();

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
        // Pede ao kernel para imprimir (Syscall 2)
        sys_puts("Hello, "); 
        
        // Pede ao kernel para dormir (Syscall 3).
        // Isso coloca a tarefa em estado BLOCKED e libera a CPU imediatamente.
        sys_sleep(500);
    }
}

// Tarefa B: Outro cidadão comportado
void task_b(void) {
    while (1) {
        sys_puts("World!\n"); 
        sys_sleep(500);
    }
}

// Tarefa Idle: O scheduler escolhe esta tarefa quando ninguém mais quer rodar.
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
                // Usado para UART RX, Botões, etc.
                {
                    uint32_t source = hal_plic_claim();
                    if (source) hal_plic_complete(source);
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
            hal_uart_puts("System Halted." ANSI_RESET);
            while(1); // Trava o sistema para análise
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
    
    hal_uart_init();                  // Inicializa UART
    hal_uart_puts("\033[2J\033[H");   // Limpar tela

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

    // ----------------------------------------------------------------------------------
    // FASE 3: Criação de Processos
    // ----------------------------------------------------------------------------------

    log_info("Initializing Process Scheduler...");
    scheduler_init();
    
    // Cria as tarefas do usuário (Pilha, Contexto, TCB)
    task_create(task_a, "Task A", 1);
    task_create(task_b, "Task B", 1);
    
    // Cria a tarefa de background (obrigatória para o scheduler não falhar)
    task_create(task_idle, "Idle", 0);
    
    hal_uart_putc('\n');
    hal_uart_puts(ANSI_GREEN ">>> AXON KERNEL IS READY. <<<\n\r" ANSI_RESET);
    
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