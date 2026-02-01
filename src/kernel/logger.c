#include "../../include/logger.h"
#include "../../include/hal_uart.h"

// ============================================================================
//  FUNÇÕES AUXILIARES DE LOG
// ============================================================================

void log_info(const char* msg) {
    hal_uart_puts(ANSI_CYAN "[ INFO  ] " ANSI_RESET);
    hal_uart_puts(msg);
    hal_uart_puts("\n\r");
}

void log_ok(const char* msg) {
    hal_uart_puts(ANSI_GREEN "[ OK    ] " ANSI_RESET);
    hal_uart_puts(msg);
    hal_uart_puts("\n\r");
}

void log_warn(const char* msg) {
    hal_uart_puts(ANSI_YELLOW "[ WARN  ] " ANSI_RESET);
    hal_uart_puts(msg);
    hal_uart_puts("\n\r");
}

void log_sched(const char* msg) {
    hal_uart_puts(ANSI_YELLOW "[ SCHED ] " ANSI_RESET);
    hal_uart_puts(msg);
}

void print_hex(unsigned int val) {
    char hex_chars[] = "0123456789ABCDEF";
    hal_uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        hal_uart_putc(hex_chars[(val >> i) & 0xF]);
    }
}