#include <stddef.h>
#include "../../include/apps/commands.h"
#include "../../include/apps/shell_utils.h"
#include "../../include/sys/syscall.h"

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

extern volatile int g_editor_mode;

// ============================================================================
// WRAPPERS DE SYSCALL (Ponte Shell -> Kernel)
// ============================================================================

int sys_fs_create(const char *name) {
    int ret; 
    asm volatile("li a7, %1; mv a0, %2; ecall; mv %0, a0" 
                 : "=r"(ret) : "i"(SYS_FS_CREATE), "r"(name) : "a0", "a7");
    return ret;
}

int sys_fs_write(const char *name, const char *data, int len) {
    int ret; 
    asm volatile("li a7, %1; mv a0, %2; mv a1, %3; mv a2, %4; ecall; mv %0, a0" 
                 : "=r"(ret) : "i"(SYS_FS_WRITE), "r"(name), "r"(data), "r"(len) : "a0", "a1", "a2", "a7");
    return ret;
}

int sys_fs_read(const char *name, char *buf, int max) {
    int ret; 
    asm volatile("li a7, %1; mv a0, %2; mv a1, %3; mv a2, %4; ecall; mv %0, a0" 
                 : "=r"(ret) : "i"(SYS_FS_READ), "r"(name), "r"(buf), "r"(max) : "a0", "a1", "a2", "a7");
    return ret;
}

int sys_fs_list(char *buf, int max) {
    int ret; 
    asm volatile("li a7, %1; mv a0, %2; mv a1, %3; ecall; mv %0, a0" 
                 : "=r"(ret) : "i"(SYS_FS_LIST), "r"(buf), "r"(max) : "a0", "a1", "a7");
    return ret;
}

int sys_fs_delete(const char *name) {
    int ret; 
    asm volatile("li a7, %1; mv a0, %2; ecall; mv %0, a0" 
                 : "=r"(ret) : "i"(SYS_FS_DELETE), "r"(name) : "a0", "a7");
    return ret;
}

// ============================================================================
// COMANDOS DO SHELL
// ============================================================================

// --- LS: Lista arquivos ---
void cmd_ls(const char *args) {
    (void)args;
    char buf[256];
    
    // Limpa buffer
    for(int i=0;i<256;i++) buf[i]=0;

    int res = sys_fs_list(buf, 256);
    
    if (res < 0) {
        safe_puts(SH_RED "Error reading directory.\n" SH_RESET);
        return;
    }

    safe_puts(SH_BOLD "\n  FILES:\n" SH_RESET);
    safe_puts(SH_GRAY "  -------------------\n" SH_RESET);
    
    if (buf[0] == 0) {
        safe_puts("  (empty)\n");
    } else {
        safe_puts(buf); // O Kernel já formata com \n
    }
    safe_puts("\n");
}

// --- TOUCH: Cria arquivo vazio ---
void cmd_touch(const char *args) {
    if (!args || !*args) {
        safe_puts("Usage: touch <filename>\n");
        return;
    }
    
    int res = sys_fs_create(args);
    if (res == 0) safe_puts("File created.\n");
    else if (res == -1) safe_puts("Error: File exists.\n");
    else safe_puts("Error: Disk full or invalid name.\n");
}

// --- RM: Deleta arquivo ---
void cmd_rm(const char *args) {
    if (!args || !*args) {
        safe_puts("Usage: rm <filename>\n");
        return;
    }

    int res = sys_fs_delete(args);
    if (res == 0) safe_puts("File deleted.\n");
    else safe_puts("Error: File not found.\n");
}

// --- CAT: Lê e imprime arquivo ---
void cmd_cat(const char *args) {
    if (!args || !*args) {
        safe_puts("Usage: cat <filename>\n");
        return;
    }

    char buf[512]; // Buffer temporário na pilha
    int len = sys_fs_read(args, buf, 511);
    
    if (len < 0) {
        safe_puts("File not found.\n");
    } else {
        buf[len] = 0; // Garante fim da string
        safe_puts(buf);
        safe_puts("\n");
    }
}

// --- WRITE: Escreve texto simples (Ex: write notas.txt OlaMundo) ---
void cmd_write_file(const char *args) {
    if (!args) { safe_puts("Usage: write <file> <data>\n"); return; }
    
    char name[32];
    char *data = NULL;
    
    // Parser manual simples para separar nome e conteúdo
    int i=0;
    while(args[i] && args[i] != ' ' && i < 31) {
        name[i] = args[i];
        i++;
    }
    name[i] = 0;
    
    // Pula espaços até achar o dado
    if (args[i] == ' ') data = (char*)&args[i+1];
    
    if (!data) {
        safe_puts("Error: No data provided.\n");
        return;
    }

    // Calcula tamanho da string de dados
    int len = 0; while(data[len]) len++;
    
    int res = sys_fs_write(name, data, len);
    if (res >= 0) safe_puts("Written.\n");
    else safe_puts("Error writing file (Disk full?).\n");
}

static char edit_buffer[2048]; // 2KB para edição 
extern char shell_getc(void);  // A função que criamos acima

// Helper: Imprime buffer convertendo \n em \r\n para o terminal não fazer "escada"
static void safe_print_buffer(const char *buf) {
    while (*buf) {
        if (*buf == '\n') {
            safe_puts("\r\n"); // Força o retorno de carro visual
        } else {
            char s[2] = {*buf, 0};
            safe_puts(s);
        }
        buf++;
    }
}

// Helper: Redesenha a interface inteira
static void nano_redraw(const char *filename, const char *status_msg) {
    // 1. Limpa tela e vai para Home
    safe_puts("\033[2J\033[H"); 
    
    // 2. Cabeçalho
    safe_puts(SH_CYAN " AXON NANO " SH_RESET "   File: ");
    safe_puts(filename);
    
    if (status_msg) safe_puts(status_msg);

    safe_puts("\n" SH_GRAY " [Ctrl+W] Save   [Ctrl+Q] Quit\n" SH_RESET); 
    safe_puts("------------------------------------------------\r\n");
    
    // 3. Conteúdo (com correção visual de quebra de linha)
    safe_print_buffer(edit_buffer);
}

void cmd_edit(const char *args) {
    if (!args || !*args) { safe_puts("Usage: edit <file>\n"); return; }

    g_editor_mode = 1; // Pausa Uptime

    // Carrega arquivo e limpa buffer
    for(int i=0; i<2048; i++) edit_buffer[i] = 0;
    int len = sys_fs_read(args, edit_buffer, 2047);
    if (len < 0) len = 0;
    edit_buffer[len] = 0;

    nano_redraw(args, NULL);

    while (1) {
        char c = shell_getc(); 

        // --- SAIR: Ctrl+Q ---
        if (c == 17) break; 
        
        // --- SALVAR: Ctrl+W ---
        else if (c == 23) {
            sys_fs_write(args, edit_buffer, len);
            nano_redraw(args, SH_GREEN "  [SAVED]" SH_RESET);
        }
        
        // --- BACKSPACE ---
        else if (c == 127 || c == 8) {
            if (len > 0) {
                len--;
                char deleted = edit_buffer[len];
                edit_buffer[len] = 0;
                
                // Se apagou um \n, o texto de baixo precisa subir visualmente.
                // Redesenhar tudo é a forma mais segura sem controle de cursor complexo.
                if (deleted == '\n') {
                    nano_redraw(args, NULL);
                } else {
                    safe_puts("\b \b");
                }
            }
        }
        
        // --- ENTER ---
        else if (c == '\r') {
            if (len < 2047) {
                edit_buffer[len++] = '\n'; // Salva apenas \n no arquivo
                edit_buffer[len] = 0;
                safe_puts("\r\n");         // Na tela, imprime \r\n (Enter Visual Correto)
            }
        }
        
        // --- TEXTO ---
        else if (c >= 32 && c <= 126) {
            if (len < 2047) {
                edit_buffer[len++] = c;
                edit_buffer[len] = 0;
                char s[2] = {c, 0};
                safe_puts(s);
            }
        }
    }

    // SAIDA: Limpa tela e prepara espaço para o prompt 
    clear_screen();
    
    // Libera Uptime
    g_editor_mode = 0;

}