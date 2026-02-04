#ifndef MM_H
#define MM_H

#include <stddef.h>
#include <stdint.h>

// Inicializa o Heap a partir do endereço 'start_addr' com tamanho 'size'
void kmalloc_init(void* start_addr, uint32_t size);

// Aloca 'size' bytes. Retorna NULL se não houver espaço.
void* kmalloc(uint32_t size);

// Libera a memória apontada por 'ptr'.
uint8_t kfree(void* ptr);

// Retorna o total de bytes livres (para diagnóstico)
uint32_t kget_free_memory(void);

// Realiza a desfragmentação do heap, fundindo blocos livres adjacentes
void kheap_defrag(void);

#endif