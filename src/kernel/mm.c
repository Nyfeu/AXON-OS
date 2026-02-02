#include "../../include/mm.h"
#include "../../include/hal_uart.h"

// Cabeçalho de cada bloco de memória
// Cada alocação terá este "ticket" escondido antes dos dados.
typedef struct block_meta {
    size_t size;             // Tamanho do dado (sem contar este header)
    struct block_meta *next; // Próximo bloco na lista
    int free;                // 1 = Livre, 0 = Ocupado
} block_t;

// Tamanho do cabeçalho alinhado
#define BLOCK_SIZE sizeof(block_t)

static void debug_hex(uint32_t val) {
    char buf[12];
    const char hex[] = "0123456789ABCDEF";
    buf[0]='0'; buf[1]='x';
    for(int i=7; i>=0; i--) buf[2+(7-i)] = hex[(val>>(i*4))&0xF];
    buf[10]=0;
    hal_uart_puts(buf);
}

static void *heap_start = NULL;
static block_t *free_list = NULL;

// Inicializa o gerenciador
void kmalloc_init(void* start_addr, uint32_t size) {
    // Alinha o endereço inicial em 4 bytes (Word Align)
    uint32_t addr = (uint32_t)start_addr;
    if (addr & 3) {
        uint32_t padding = 4 - (addr & 3);
        addr += padding;
        size -= padding;
    }

    heap_start = (void*)addr;
    
    // Cria o "Bloco Gênesis": Um único bloco gigante livre
    free_list = (block_t*)heap_start;
    free_list->size = size - BLOCK_SIZE;
    free_list->next = NULL;
    free_list->free = 1;
}

// Aloca memória
void* kmalloc(uint32_t size) {
    block_t *curr = free_list;

    hal_uart_puts("[DEBUG] kmalloc req: "); 
    debug_hex(size);
    hal_uart_puts(" | free_list addr: ");
    debug_hex((uint32_t)free_list);
    hal_uart_puts("\n\r");
    
    // Alinha o tamanho solicitado em 4 bytes (segurança para RISC-V)
    if (size & 3) size += 4 - (size & 3);

    while (curr) {

        hal_uart_puts("   > Inspecting Block at: "); debug_hex((uint32_t)curr);
        hal_uart_puts(" Size: "); debug_hex(curr->size);
        hal_uart_puts(" Free: "); debug_hex(curr->free);
        hal_uart_puts("\n\r");

        // Procura um bloco livre que caiba o dado
        if (curr->free && curr->size >= size) {
            
            // SPLIT: Se o bloco for muito maior que o necessário, divide em dois
            if (curr->size > (size + BLOCK_SIZE + 8)) { // +8 de margem
                block_t *new_block = (block_t*)((uint8_t*)curr + BLOCK_SIZE + size);
                
                new_block->size = curr->size - size - BLOCK_SIZE;
                new_block->next = curr->next;
                new_block->free = 1;
                
                curr->size = size;
                curr->next = new_block;
            }
            
            curr->free = 0; // Marca como ocupado

            hal_uart_puts("[DEBUG] Block found! returning ptr.\n\r");
            
            // Retorna o ponteiro para a ÁREA DE DADOS (pula o header)
            return (void*)((uint8_t*)curr + BLOCK_SIZE);
        }
        curr = curr->next;
    }
    
    hal_uart_puts("[DEBUG] No block found (End of List).\n\r");
    return NULL; // Out of Memory (OOM)
}

// Libera memória
void kfree(void* ptr) {
    if (!ptr) return;
    
    // Recupera o header (voltando bytes para trás)
    block_t *block = (block_t*)((uint8_t*)ptr - BLOCK_SIZE);
    block->free = 1;
    
    // TODO: Merge (Juntar blocos vizinhos livres para desfragmentar)
    // Por enquanto, apenas marcar como livre já permite reuso imediato.
}

// Diagnóstico
uint32_t kget_free_memory(void) {
    uint32_t total = 0;
    block_t *curr = free_list;
    while (curr) {
        if (curr->free) total += curr->size;
        curr = curr->next;
    }
    return total;
}