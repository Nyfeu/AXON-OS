#ifndef LOGGER_H
#define LOGGER_H

// ============================================================================
// Definições de Cores para o Terminal (Debug Visual)
// ============================================================================

#define ANSI_RESET  "\033[0m"
#define ANSI_CYAN   "\033[36m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED    "\033[31m"
#define ANSI_BOLD   "\033[1m"

// ============================================================================
//  FUNÇÕES AUXILIARES DE LOG
// ============================================================================

void log_info(const char* msg);
void log_sched(const char* msg);
void log_ok(const char* msg);
void log_warn(const char* msg);
void print_hex(unsigned int val);

#endif /* LOGGER_H */