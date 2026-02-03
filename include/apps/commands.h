#ifndef COMMANDS_H
#define COMMANDS_H

// Protótipos dos comandos disponíveis no shell
void cmd_help(const char *args);
void cmd_clear(const char *args);
void cmd_reboot(const char *args);
void cmd_panic(const char *args);
void cmd_ps(const char *args);
void cmd_memtest(const char *args);
void cmd_peek(const char *args);
void cmd_poke(const char *args);
void cmd_heap(const char *args);

#endif
