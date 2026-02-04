#include "../../include/kernel/mm.h"
#include "../../include/hal/hal_uart.h"

// Cabeçalho de cada bloco de memória
// Cada alocação terá este "ticket" escondido antes dos dados.
typedef struct block_meta {
    size_t size;             // Tamanho do dado (sem contar este header)
    struct block_meta *next; // Próximo bloco na lista
    int free;                // 1 = Livre, 0 = Ocupado
    uint32_t canary;         // Para detecção de corrupção
} block_t;

// Tamanho do cabeçalho alinhado
#define BLOCK_SIZE  sizeof(block_t)
#define CANRY_VALUE 0xCAFEBABE

static void debug_hex(uint32_t val) {
    char buf[12];
    const char hex[] = "0123456789ABCDEF";
    buf[0]='0'; buf[1]='x';
    for(int i=7; i>=0; i--) buf[2+(7-i)] = hex[(val>>(i*4))&0xF];
    buf[10]=0;
    hal_uart_puts(buf);
}

static void *heap_start = NULL;
static void *heap_end   = NULL;
static block_t *heap_head = NULL;

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
    heap_end   = (void*)(addr + size);
    
    // Cria o "Bloco Gênesis": Um único bloco gigante livre
    heap_head = (block_t*)heap_start;
    heap_head->size = size - BLOCK_SIZE;
    heap_head->next = NULL;
    heap_head->free = 1;
    heap_head->canary = CANRY_VALUE;

}

// Aloca memória
void* kmalloc(uint32_t size) {
    block_t *curr = heap_head;
    
    // Alinha o tamanho solicitado em 4 bytes (segurança para RISC-V)
    if (size & 3) size += 4 - (size & 3);

    while (curr) {

        hal_uart_puts("Inspecting Block at: "); debug_hex((uint32_t)curr);
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
                new_block->canary = CANRY_VALUE;
                
                curr->size = size;
                curr->next = new_block;
            }
            
            curr->free = 0; // Marca como ocupado
            
            // Retorna o ponteiro para a ÁREA DE DADOS (pula o header)
            return (void*)((uint8_t*)curr + BLOCK_SIZE);
        }
        curr = curr->next;
    }
    
    return NULL; // Out of Memory (OOM)
}

// Libera memória
uint8_t kfree(void* ptr) {
    if (!ptr) return -1;

    // 1. Verificação de Limites (Safety Check)
    // O ponteiro deve estar estritamente dentro da área do Heap
    if (ptr < heap_start || ptr >= heap_end) {
        hal_uart_puts("[MM] Error: Pointer out of heap bounds!\n\r");
        return -1;
    }

    // 2. Verificação de Alinhamento
    if (((uint32_t)ptr & 3) != 0) {
        hal_uart_puts("[MM] Error: Invalid pointer alignment!\n\r");
        return -1;
    }

    // Recupera o cabeçalho
    block_t *block = (block_t*)((uint8_t*)ptr - BLOCK_SIZE);

    // 3. Verificação de Integridade (Magic Number)
    if (block->canary != CANRY_VALUE) {
        hal_uart_puts("[MM] Error: Block corruption detected (Bad Canary)!\n\r");
        return -1;
    }

    // Marca como livre
    block->free = 1;
    return 0;
}

// Algoritmo de Fusão de Blocos (Coalescing)
void kheap_defrag(void) {
    block_t *curr = heap_head;
    int merged_count = 0;

    while (curr && curr->next) {
        
        // Se EU sou livre E o meu VIZINHO é livre...
        if (curr->free && curr->next->free) {
            
            // ...Engulo o vizinho!
            // Meu tamanho = Meu tamanho + tamanho do vizinho + cabeçalho do vizinho
            curr->size += curr->next->size + BLOCK_SIZE;
            
            // Pulo o vizinho na lista encadeada (ele deixa de existir logicamente)
            curr->next = curr->next->next;
            
            merged_count++;
            
            // IMPORTANTE: Não avançamos 'curr' aqui. 
            // Motivo: O novo blocão formado pode ser vizinho de *outro* bloco livre à frente.
            // Ficamos parados no mesmo lugar e verificamos novamente na próxima iteração do while.
            
        } else {
            // Se não deu para fundir, vida que segue.
            curr = curr->next;
        }
    }
    
    if (merged_count > 0) {
        hal_uart_puts("[MM] Defrag: Merged ");
        // print_dec(merged_count); // Se tiver print_dec disponível
        hal_uart_puts(" blocks.\n\r");
    }
}

// Diagnóstico
uint32_t kget_free_memory(void) {
    uint32_t total = 0;
    block_t *curr = heap_head;
    while (curr) {
        if (curr->free) total += curr->size;
        curr = curr->next;
    }
    return total;
}

// Debug Atualizado com Canary Check
void kheap_dump(void) {
    hal_uart_puts("\n  HEAP MAP (Start: ");
    debug_hex((uint32_t)heap_start); 
    hal_uart_puts(")\n");
    
    hal_uart_puts("  ------------------------------------------------------------------\n");
    hal_uart_puts("  HEAD ADDR   DATA ADDR   CANARY ADDR   SIZE          STATUS   CHK\n");
    hal_uart_puts("  ------------------------------------------------------------------\n");

    block_t *curr = heap_head;

    while (curr) {
        hal_uart_puts("  "); debug_hex((uint32_t)curr);            
        hal_uart_puts("  "); debug_hex((uint32_t)curr + BLOCK_SIZE); 
        
        // Mostra onde o Canary mora (Head + 12 bytes)
        hal_uart_puts("  "); debug_hex((uint32_t)&curr->canary);
        
        hal_uart_puts("    "); debug_hex(curr->size);                
        hal_uart_puts(curr->free ? "    FREE     " : "    USED     "); 
        
        if (curr->canary == CANRY_VALUE) hal_uart_puts("OK\n");
        else hal_uart_puts("ERR\n");

        curr = curr->next;
    }

    hal_uart_puts("  ------------------------------------------------------------------\n");
    hal_uart_puts("  Total Free: ");
    debug_hex(kget_free_memory()); 
    hal_uart_puts("\n\n");
}