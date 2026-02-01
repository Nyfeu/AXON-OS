#ifndef MUTEX_H
#define MUTEX_H

#include <stdint.h>

// ======================================================================================
// Estrutura do MUTEX
// ======================================================================================

// Responsável pela exclusão mútua em regiões críticas de código.
// Isso torna o acesso à recursos compartilhados mais seguro.

typedef struct {

    volatile int locked; // 0 = Livre (Verde), 1 = Trancado (Vermelho)
    uint32_t owner_tid;  // ID da tarefa que está com a chave (para evitar que outra destranque)
    
} mutex_t;

// Função auxiliar para inicializar (deve ser chamada antes de usar)
static inline void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner_tid = 0;
}

#endif