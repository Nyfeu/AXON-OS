#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdint.h>

#define BUFFER_SIZE 128

typedef struct {
    volatile uint8_t data[BUFFER_SIZE];
    volatile uint32_t head; // Onde escrever (ISR)
    volatile uint32_t tail; // Onde ler (Task)
} cbuf_t;

static inline void cbuf_init(cbuf_t *cb) {
    cb->head = 0;
    cb->tail = 0;
}

static inline int cbuf_push(cbuf_t *cb, uint8_t val) {
    uint32_t next = (cb->head + 1) % BUFFER_SIZE;
    if (next == cb->tail) return 0; // Cheio
    cb->data[cb->head] = val;
    cb->head = next;
    return 1;
}

static inline int cbuf_pop(cbuf_t *cb, uint8_t *val) {
    if (cb->head == cb->tail) return 0; // Vazio
    *val = cb->data[cb->tail];
    cb->tail = (cb->tail + 1) % BUFFER_SIZE;
    return 1;
}

#endif