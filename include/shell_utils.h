#ifndef SHELL_UTILS_H
#define SHELL_UTILS_H

#include <stdint.h>
#include "mutex.h"
#include "syscall.h"

// ======================================================================================
// Definições de CORES para o SHELL
// ======================================================================================

#define SH_RESET   "\033[0m"
#define SH_CYAN    "\033[36m"
#define SH_GREEN   "\033[32m"
#define SH_YELLOW  "\033[33m"
#define SH_RED     "\033[31m"
#define SH_BOLD    "\033[1m"
#define SH_GRAY    "\033[90m"

// ======================================================================================
// FUNÇÕES AUXILIARES 
// ======================================================================================

// Mutex para uso da UART (definido em apps.c)
extern mutex_t uart_mutex;

// safe_puts: Função thread-safe para escrever na UART
static inline void safe_puts(const char* s) {
    while (sys_mutex_lock(&uart_mutex) == 0) sys_yield(); 
    sys_puts(s);
    sys_mutex_unlock(&uart_mutex);
}

// int_to_str: Converte inteiros 0-99 para string
static inline void int_to_str(int val, char* buf) {
    if (val < 10) {
        buf[0] = '0';
        buf[1] = val + '0';
        buf[2] = 0;
    } else {
        buf[0] = (val / 10) + '0';
        buf[1] = (val % 10) + '0';
        buf[2] = 0;
    }
}

// val_to_hex: Converte uint32_t para string hexadecimal (formato "0xHHHHHHHH")
static inline void val_to_hex(uint32_t val, char* out_buf) {
    const char hex_chars[] = "0123456789ABCDEF";
    out_buf[0] = '0'; 
    out_buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        out_buf[2 + (7-i)] = hex_chars[(val >> (i*4)) & 0xF];
    }
    out_buf[10] = 0;
}

// sys_strcmp: Compara strings
static inline int sys_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

#endif
