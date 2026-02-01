#include "../../../include/hal_dma.h"

int hal_dma_is_busy(void) { 
    return 0; 
}

void hal_dma_memcpy(uint32_t src, uint32_t dst, uint32_t size, int fixed) {
    // Simulação funcional: Copia via software para não quebrar lógica
    uint32_t *s = (uint32_t *)src;
    uint32_t *d = (uint32_t *)dst;
    for(uint32_t i=0; i<size; i++) {
        if(fixed) *d = *s++;
        else *d++ = *s++;
    }
}