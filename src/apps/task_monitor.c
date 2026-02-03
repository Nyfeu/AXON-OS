#include "../../include/sys/syscall.h"
#include "../../include/apps/shell_utils.h"

// ======================================================================================
// TASK: MONITOR (Uptime + Spinner)
// ======================================================================================

// Faz monitoramento simples do sistema: uptime e spinner animado

void task_monitor(void) {
    int seconds = 0;
    int minutes = 0;
    char s_str[4], m_str[4];
    const char spinner[] = "|/-\\";
    int spin_idx = 0;

    while (1) {
        int_to_str(minutes, m_str);
        int_to_str(seconds, s_str);

        sys_mutex_lock(&uart_mutex); 
        sys_puts("\0337\033[1;1H"); 
        sys_puts("Uptime: ");
        sys_puts(m_str); sys_puts(":"); sys_puts(s_str);
        sys_puts("  ["); 
        char spin_char[2] = { spinner[spin_idx], 0 };
        sys_puts(SH_CYAN); sys_puts(spin_char); sys_puts(SH_RESET);
        sys_puts("]");
        sys_puts("\0338");
        sys_mutex_unlock(&uart_mutex);

        spin_idx = (spin_idx + 1) % 4;
        
        if (spin_idx == 0) {
            seconds++;
            if (seconds >= 60) {
                seconds = 0;
                minutes++;
            }
        }

        sys_sleep(250);
    }
}
