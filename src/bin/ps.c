#include "apps/shell_utils.h"

// ======================================================================================
// COMANDO: PS (Process Status)
// ======================================================================================

void cmd_ps(const char *args) {

    (void)args; // Ignora os argumentos

    task_info_t list[8]; 
    int count = sys_get_tasks(list, 8);
    
    safe_puts(SH_BOLD "\n  PID   NAME            PRIO   STATE         SP          WAKE_TIME\n" SH_RESET);
    safe_puts(SH_GRAY "  --------------------------------------------------------------------\n" SH_RESET);
    
    for (int i = 0; i < count; i++) {
        char pid_str[4]; pid_str[0] = list[i].id + '0'; pid_str[1] = 0;
        
        const char *state_str;
        switch(list[i].state) {
            case 0: state_str = SH_GREEN  "READY     " SH_RESET; break;
            case 1: state_str = SH_CYAN   "RUNNING   " SH_RESET; break;
            case 2: state_str = SH_YELLOW "WAITING   " SH_RESET; break;
            case 3: state_str = SH_RED    "SUSPENDED " SH_RESET; break;
            default: state_str =          "UNKNOWN   "         ; break;
        }

        char sp_str[12], wake_str[12];
        val_to_hex(list[i].sp, sp_str);
        val_to_hex((uint32_t)list[i].wake_time, wake_str);

        safe_puts("  "); safe_puts(pid_str); safe_puts("     ");
        safe_puts(list[i].name);
        
        int len = 0; while(list[i].name[len]) len++;
        for(int s=0; s<(16-len); s++) safe_puts(" ");
        
        safe_puts(list[i].priority ? "1      " : "0      ");
        safe_puts(state_str); safe_puts("    ");
        safe_puts(sp_str); safe_puts("  ");
        safe_puts(list[i].state != 2 ? "-         " : wake_str);

        safe_puts("\n");

    }

    safe_puts("\n");

}
