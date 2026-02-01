// ======================================================================================
//  AXON RTOS - KERNEL MAIN
// ======================================================================================
//  Adaptado para a nova Hardware Abstraction Layer (HAL)
// ======================================================================================

#include "../../include/hal_uart.h"
#include "../../include/hal_timer.h"
#include "../../include/hal_irq.h"
#include "../../include/hal_plic.h" 

// ======================================================================================
//  DEFINIÇÕES DE CORES ANSI
// ======================================================================================
#define ANSI_RESET  "\033[0m"
#define ANSI_CYAN   "\033[36m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED    "\033[31m"
#define ANSI_BOLD   "\033[1m"

// ======================================================================================
//  FUNÇÕES AUXILIARES DE LOG (Usando hal_uart)
// ======================================================================================

void log_info(const char* msg) {
    hal_uart_puts(ANSI_CYAN "[ INFO ] " ANSI_RESET);
    hal_uart_puts(msg);
    hal_uart_puts("\n\r");
}

void log_ok(const char* msg) {
    hal_uart_puts(ANSI_GREEN "[  OK  ] " ANSI_RESET);
    hal_uart_puts(msg);
    hal_uart_puts("\n\r");
}

void log_warn(const char* msg) {
    hal_uart_puts(ANSI_YELLOW "[ WARN ] " ANSI_RESET);
    hal_uart_puts(msg);
    hal_uart_puts("\n\r");
}

void print_hex(unsigned int val) {
    char hex_chars[] = "0123456789ABCDEF";
    hal_uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        hal_uart_putc(hex_chars[(val >> i) & 0xF]);
    }
}

// ======================================================================================
//  TRATAMENTO DE INTERRUPÇÕES (Compatível com trap.s)
// ======================================================================================

// Definido no trap.s
extern void trap_entry();

// Intervalo do Timer: 100ms (assumindo clock de 10MHz no QEMU/FPGA simples)
// Ajuste conforme seu clock real. Se for 100MHz, use 10000000.
#define TICK_DELTA_CYCLES 1000000 

// Handler C chamado pelo assembly trap.s
void trap_handler(unsigned int mcause, unsigned int mepc) {

    int is_interrupt = (mcause >> 31);
    int cause_code = mcause & 0x7FFFFFFF;

    if (is_interrupt) {
        switch (cause_code) {
            case 7: // Machine Timer Interrupt
                hal_uart_puts("."); // Print discreto para não floodar
                
                // Re-programar o próximo tick usando a nova API inline
                hal_timer_set_irq_delta(TICK_DELTA_CYCLES);
                break;

            case 11: // Machine External Interrupt (PLIC)
                // Se estiver usando o irq_dispatch, chamaria ele aqui.
                // Por enquanto, apenas avisamos e limpamos.
                {
                    uint32_t source = hal_plic_claim();
                    if (source) {
                        hal_uart_puts("[IRQ EXT]");
                        hal_plic_complete(source);
                    }
                }
                break;

            default:
                hal_uart_puts(ANSI_RED "\n\r[FAIL] Unknown Interrupt: ");
                print_hex(cause_code);
                hal_uart_puts("\n\r" ANSI_RESET);
                break;
        }

    } else {
        // EXCEÇÃO SÍNCRONA (CRASH)
        hal_uart_puts(ANSI_RED "\n\r[CRIT] EXCEPTION DETECTED!\n\r");
        hal_uart_puts("   > MCAUSE: "); print_hex(mcause); hal_uart_puts(" (Cause)\n\r");
        hal_uart_puts("   > MEPC:   "); print_hex(mepc);   hal_uart_puts(" (Address)\n\r");
        hal_uart_puts("System Halted." ANSI_RESET);
        while(1);
    }
}

// ======================================================================================
//  KERNEL MAIN
// ======================================================================================

void kernel_main() {
    // 1. Inicializar Hardware (UART)
    hal_uart_init();
    
    // Limpar tela
    hal_uart_puts("\033[2J\033[H");

    // 1. ASCII ART BANNER
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

    log_info("Boot sequence initiated...");
    
    // 2. Configurar Interrupções
    log_info("Configuring Trap Vector Table...");
    
    // Configura mtvec para o nosso trap_entry (Assembly)
    // Nota: Usamos a macro do hal_irq.h para garantir alinhamento
    hal_irq_set_handler(trap_entry);
    log_ok("Trap handler registered at 'trap_entry'");

    // 3. Configurar Timer
    log_info("Starting System Timer...");
    // Configura o primeiro disparo
    hal_timer_set_irq_delta(TICK_DELTA_CYCLES);
    
    // Habilita interrupção de Timer (MIE.MTIE)
    hal_irq_mask_enable(IRQ_M_TIMER);
    log_ok("Timer armed.");

    // 4. Habilitar Interrupções Globais
    hal_irq_global_enable();
    log_ok("Global Interrupts Enabled (MIE=1)");

    hal_uart_puts(ANSI_GREEN ">>> AXON KERNEL IS READY. <<<\n\r" ANSI_RESET);
    hal_uart_puts(ANSI_CYAN "Waiting for events...\n\r" ANSI_RESET);

    // Loop Infinito
    while (1) {
        // Busy wait (compatível com FPGA e QEMU)
        // No futuro, usar 'wfi' apenas se não for FPGA softcore
        asm volatile("nop");
    }
}