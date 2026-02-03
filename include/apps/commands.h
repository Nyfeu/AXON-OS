#ifndef COMMANDS_H
#define COMMANDS_H

// Sistema
void cmd_help(const char *args);
void cmd_clear(const char *args);
void cmd_reboot(const char *args);
void cmd_panic(const char *args);

// Processos
void cmd_ps(const char *args);
void cmd_stop(const char *args);
void cmd_resume(const char *args);

// Memória
void cmd_memtest(const char *args);
void cmd_heap(const char *args);
void cmd_peek(const char *args);
void cmd_poke(const char *args);
void cmd_alloc(const char *args);
void cmd_free(const char *args);

#endif
