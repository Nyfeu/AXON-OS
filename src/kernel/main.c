#include "../../include/uart.h"
#include "../../include/timer.h"

// ======================================================================================
//  DEFINIÇÕES DE CORES ANSI (Terminal Style)
// ======================================================================================

#define ANSI_RESET  "\033[0m"
#define ANSI_CYAN   "\033[36m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED    "\033[31m"
#define ANSI_BOLD   "\033[1m"

// ======================================================================================
//  FUNÇÕES AUXILIARES DE LOG
// ======================================================================================

void log_info(const char* msg) {
    uart_puts(ANSI_CYAN "[ INFO ] " ANSI_RESET);
    uart_puts(msg);
    uart_puts("\n\r");
}

void log_ok(const char* msg) {
    uart_puts(ANSI_GREEN "[  OK  ] " ANSI_RESET);
    uart_puts(msg);
    uart_puts("\n\r");
}

void log_warn(const char* msg) {
    uart_puts(ANSI_YELLOW "[ WARN ] " ANSI_RESET);
    uart_puts(msg);
    uart_puts("\n\r");
}

void print_hex(unsigned int val) {
    char hex_chars[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(hex_chars[(val >> i) & 0xF]);
    }
}

// ======================================================================================
//  TRATAMENTO DE INTERRUPÇÕES
// ======================================================================================

// Definido no trap.s
extern void trap_entry();

// Gerenciador de interrupções
void trap_handler(unsigned int mcause, unsigned int mepc) {

    int is_interrupt = (mcause >> 31);
    int cause_code = mcause & 0x7FFFFFFF;

    if (is_interrupt) {
        if (cause_code == 7) { 
            
            // Machine Timer Interrupt
            timer_handler();

        } else {

            uart_puts(ANSI_RED "\n\r[FAIL] Unknown Interrupt: ");
            print_hex(cause_code);
            uart_puts("\n\r" ANSI_RESET);

        }

    } else {

        // TRAP SÍNCRONA (EXCEÇÃO)
        uart_puts(ANSI_RED "\n\r[CRIT] EXCEPTION DETECTED!\n\r");
        uart_puts("   > MCAUSE: "); print_hex(mcause); uart_puts(" (Cause)\n\r");
        uart_puts("   > MEPC:   "); print_hex(mepc);   uart_puts(" (Address)\n\r");
        uart_puts("System Halted." ANSI_RESET);
        while(1);

    }

}

// ======================================================================================
//  KERNEL MAIN
// ======================================================================================

void kernel_main() {

    uart_init();
    
    // Limpar tela e posicionar cursor no topo
    uart_puts("\033[2J\033[H");

    // 1. ASCII ART BANNER
    uart_puts(ANSI_CYAN ANSI_BOLD);
    uart_puts("\n\r");
    uart_puts(" █████╗ ██╗  ██╗ ██████╗ ███╗   ██╗       ██████╗ ███████╗\n\r");
    uart_puts("██╔══██╗╚██╗██╔╝██╔═══██╗████╗  ██║      ██╔═══██╗██╔════╝\n\r");
    uart_puts("███████║ ╚███╔╝ ██║   ██║██╔██╗ ██║█████╗██║   ██║███████╗\n\r");
    uart_puts("██╔══██║ ██╔██╗ ██║   ██║██║╚██╗██║╚════╝██║   ██║╚════██║\n\r");
    uart_puts("██║  ██║██╔╝ ██╗╚██████╔╝██║ ╚████║      ╚██████╔╝███████║\n\r");
    uart_puts("╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝       ╚═════╝ ╚══════╝\n\r");   
    uart_puts("\n\r");
    uart_puts(ANSI_RESET);
    uart_puts("   :: AXON RTOS :: (v0.1.0-alpha) \n\r");
    uart_puts("   :: Build: RISC-V 32-bit (RV32I_Zicsr) \n\r");
    uart_puts("\n\r");

    // 2. SEQUÊNCIA DE BOOT
    log_info("Boot sequence initiated...");
    
    // Simulação de check de memória
    log_info("Probing system memory map...");
    log_ok("RAM validated: 256K available at 0x80000000");

    // Inicialização da UART
    log_info("Initializing I/O Subsystem...");
    log_ok("UART0 driver loaded (921600 baud, 8N1)");

    // Simulação de detecção da NPU (Futuro)
    log_info("Detecting Neural Processing Unit...");
    log_warn("NPU not detected (Simulation Mode Active)");

    // 3. CONFIGURAÇÃO DE INTERRUPÇÕES
    log_info("Configuring Trap Vector Table...");
    
    // mtvec
    asm volatile("csrw mtvec, %0" : : "r"(trap_entry));
    log_ok("Trap handler registered at 'trap_entry'");

    // Timer
    log_info("Calibrating System Timer (CLINT)...");
    timer_init();
    log_ok("Timer initialized (100ms interval)");

    // Habilitar Global Interrupts
    unsigned int mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    mstatus |= (1 << 3); 
    asm volatile("csrw mstatus, %0" : : "r"(mstatus));
    
    uart_puts("\n\r");
    log_ok("System Interrupts Enabled (MIE=1)");
    uart_puts(ANSI_GREEN ">>> AXON KERNEL IS READY. <<<\n\r" ANSI_RESET);
    uart_puts(ANSI_CYAN "Waiting for events...\n\r" ANSI_RESET);

    // Loop infinito
    while (1);

}

// ======================================================================================