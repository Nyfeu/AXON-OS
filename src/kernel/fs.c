#include "../../include/kernel/fs.h"
#include "../../include/kernel/mm.h"
#include "../../include/hal/hal_uart.h"

// ============================================================================
// ESTRUTURA FÍSICA DO DISCO VIRTUAL (RAM)
// ============================================================================
// Layout da Memória:
// [ SUPERBLOCK ] [ INODE BITMAP ] [ BLOCK BITMAP ] [ INODE TABLE ] [ DATA BLOCKS ... ]

static uint8_t *disk_memory = NULL; // Ponteiro para o início do nosso "HD" na RAM

// Ponteiros de conveniência para as regiões internas
static superblock_t *sb;
static uint8_t      *inode_bitmap;
static uint8_t      *block_bitmap;
static inode_t      *inode_table;
static uint8_t      *data_blocks;

// Tamanho total do disco
#define DISK_SIZE (sizeof(superblock_t) + \
                   (FS_MAX_INODES/8) + \
                   (FS_MAX_BLOCKS/8) + \
                   (sizeof(inode_t) * FS_MAX_INODES) + \
                   (FS_BLOCK_SIZE * FS_MAX_BLOCKS))

// ============================================================================
// HELPERS DE BITMAP
// ============================================================================

// Busca o primeiro bit livre (0), marca como usado (1) e retorna o índice
static int alloc_bit(uint8_t *bitmap, int size) {
    for (int i = 0; i < size; i++) {
        // Verifica bit a bit dentro do byte (simplificado: byte a byte se size < 8)
        // Como nossos bitmaps são pequenos (32 inodes = 4 bytes), podemos fazer simples:
        if ((bitmap[i/8] >> (i%8)) & 1) continue; // Já usado
        
        // Achou livre! Marca.
        bitmap[i/8] |= (1 << (i%8));
        return i;
    }
    return -1; // Disco cheio
}

static void free_bit(uint8_t *bitmap, int idx) {
    bitmap[idx/8] &= ~(1 << (idx%8));
}

// ============================================================================
// HELPERS DE ARQUIVO
// ============================================================================

// Busca um arquivo no Diretório Raiz (Inode 0)
// Retorna o índice do Inode ou -1
static int find_inode_by_name(const char *name) {
    inode_t *root = &inode_table[0]; // Inode 0 é sempre o root
    
    // O Root contém entradas do tipo dirent_t em seus blocos de dados
    for (int i = 0; i < root->blocks_cnt; i++) {
        uint16_t blk_idx = root->blocks[i];
        dirent_t *dir_block = (dirent_t *)&data_blocks[blk_idx * FS_BLOCK_SIZE];
        
        // Cada bloco pode ter várias entradas. (FS_BLOCK_SIZE / sizeof(dirent_t))
        int max_entries = FS_BLOCK_SIZE / sizeof(dirent_t);
        
        for (int j = 0; j < max_entries; j++) {
            if (dir_block[j].inode_idx != 0) { // Entrada válida (0 seria vazio/livre aqui? Não, inode 0 é root)
                // Vamos definir inode_idx=0xFFFF como entrada vazia, pois inode 0 é válido
                if (dir_block[j].inode_idx == 0xFFFF) continue;

                // Compara string
                const char *s1 = name;
                const char *s2 = dir_block[j].name;
                int match = 1;
                while (*s1 && *s2) { if (*s1++ != *s2++) { match=0; break; } }
                if (match && *s1 == *s2) return dir_block[j].inode_idx;
            }
        }
    }
    return -1;
}

// ============================================================================
// INICIALIZAÇÃO
// ============================================================================

void fs_format(void) {
    hal_uart_puts("[FS] Formatting RamDisk...\n\r");

    // 1. Zera todo o disco
    for(int i=0; i<DISK_SIZE; i++) disk_memory[i] = 0;

    // 2. Configura Superblock
    sb->magic = FS_MAGIC;
    sb->inode_count = FS_MAX_INODES;
    sb->block_count = FS_MAX_BLOCKS;
    sb->free_inodes = FS_MAX_INODES;
    sb->free_blocks = FS_MAX_BLOCKS;

    // 3. Cria o Diretório Raiz (Root)
    // Aloca Inode 0 para o Root
    int root_idx = alloc_bit(inode_bitmap, FS_MAX_INODES); // Vai retornar 0
    inode_table[root_idx].type = 2; // Diretório
    inode_table[root_idx].size = 0;
    
    // Aloca 1 bloco de dados para o Root guardar a lista de arquivos
    int blk_idx = alloc_bit(block_bitmap, FS_MAX_BLOCKS);
    inode_table[root_idx].blocks[0] = blk_idx;
    inode_table[root_idx].blocks_cnt = 1;

    // Inicializa o bloco do diretório com entradas vazias (0xFFFF)
    dirent_t *dir_entries = (dirent_t *)&data_blocks[blk_idx * FS_BLOCK_SIZE];
    int max_entries = FS_BLOCK_SIZE / sizeof(dirent_t);
    for(int i=0; i<max_entries; i++) dir_entries[i].inode_idx = 0xFFFF;
}

void fs_init(void) {
    // Aloca a memória física do disco virtual
    disk_memory = (uint8_t*)kmalloc(DISK_SIZE);
    
    if (!disk_memory) {
        hal_uart_puts("[FS] Critical: Not enough RAM for Disk!\n\r");
        return;
    }

    // Define os ponteiros baseados nos offsets
    sb = (superblock_t *)disk_memory;
    inode_bitmap = disk_memory + sizeof(superblock_t);
    block_bitmap = inode_bitmap + (FS_MAX_INODES/8);
    inode_table  = (inode_t *)(block_bitmap + (FS_MAX_BLOCKS/8));
    data_blocks  = (uint8_t *)(inode_table + FS_MAX_INODES);

    // Formata o disco (já que é volátil, sempre formata no boot)
    fs_format();
    
    hal_uart_puts("[FS] Mounted. Size: ");
    // print_dec(DISK_SIZE);
    hal_uart_puts(" bytes.\n\r");
}

// ============================================================================
// OPERAÇÕES (CREATE, WRITE, READ)
// ============================================================================

int fs_create(const char *name) {
    if (find_inode_by_name(name) != -1) return -1; // Já existe

    // 1. Aloca um Inode
    int inode_idx = alloc_bit(inode_bitmap, FS_MAX_INODES);
    if (inode_idx < 0) return -2; // Sem inodes livres

    // 2. Configura o Inode
    inode_table[inode_idx].type = 1; // Arquivo Regular
    inode_table[inode_idx].size = 0;
    inode_table[inode_idx].blocks_cnt = 0;

    // 3. Adiciona entrada no Diretório Raiz (Inode 0)
    inode_t *root = &inode_table[0];
    dirent_t *dir_entry = NULL;
    
    // Varre os blocos do root procurando vaga
    for (int i=0; i < root->blocks_cnt; i++) {
        dirent_t *entries = (dirent_t *)&data_blocks[root->blocks[i] * FS_BLOCK_SIZE];
        for (int j=0; j < (FS_BLOCK_SIZE/sizeof(dirent_t)); j++) {
            if (entries[j].inode_idx == 0xFFFF) {
                dir_entry = &entries[j];
                break;
            }
        }
        if (dir_entry) break;
    }

    if (!dir_entry) {
        // Diretório cheio (precisaria alocar mais um bloco pro root... simplificando: erro)
        free_bit(inode_bitmap, inode_idx);
        return -3; 
    }

    // Salva o nome e linka o inode
    dir_entry->inode_idx = inode_idx;
    for(int i=0; i<FS_MAX_NAME; i++) dir_entry->name[i] = name[i];
    
    return 0;
}

int fs_write(const char *name, const uint8_t *data, uint32_t len) {
    int idx = find_inode_by_name(name);
    if (idx < 0) return -1;

    inode_t *inode = &inode_table[idx];
    
    // Simplificação: Sobrescreve tudo. Libera blocos antigos.
    // (Num FS real, reutilizariamos)
    for(int i=0; i<inode->blocks_cnt; i++) {
        free_bit(block_bitmap, inode->blocks[i]);
    }
    inode->blocks_cnt = 0;
    inode->size = 0;

    // Calcula quantos blocos precisamos
    int blocks_needed = (len + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    if (blocks_needed > 6) return -2; // Arquivo muito grande para ponteiros diretos

    int bytes_written = 0;
    const uint8_t *src_ptr = data;

    for (int i=0; i<blocks_needed; i++) {
        int blk_idx = alloc_bit(block_bitmap, FS_MAX_BLOCKS);
        if (blk_idx < 0) break; // Disco cheio

        inode->blocks[i] = blk_idx;
        inode->blocks_cnt++;
        
        // Copia dados para o bloco
        uint8_t *dst_ptr = &data_blocks[blk_idx * FS_BLOCK_SIZE];
        int chunk = (len > FS_BLOCK_SIZE) ? FS_BLOCK_SIZE : len;
        
        for(int k=0; k<chunk; k++) dst_ptr[k] = *src_ptr++;
        
        len -= chunk;
        bytes_written += chunk;
        inode->size += chunk;
    }
    
    return bytes_written;
}

int fs_read(const char *name, uint8_t *buffer, uint32_t max_len) {
    int idx = find_inode_by_name(name);
    if (idx < 0) return -1;

    inode_t *inode = &inode_table[idx];
    uint32_t total_read = 0;
    
    for (int i=0; i < inode->blocks_cnt; i++) {
        if (total_read >= max_len) break;
        
        uint8_t *src = &data_blocks[inode->blocks[i] * FS_BLOCK_SIZE];
        
        // Quanto ler deste bloco?
        // Se for o último bloco, lemos apenas o que resta do 'inode->size'
        // Mas simplificando: lemos FS_BLOCK_SIZE ou o que cabe no buffer
        
        int chunk = FS_BLOCK_SIZE;
        // Ajuste fino para não ler lixo além do EOF seria necessário aqui
        // Mas como zeramos o disco na formatação, ok.
        
        for(int k=0; k<chunk && total_read < max_len && total_read < inode->size; k++) {
            buffer[total_read++] = src[k];
        }
    }
    return total_read;
}

int fs_list(char *buffer, uint32_t max_len) {

    inode_t *root = &inode_table[0];
    uint32_t pos = 0;
    
    for (int i = 0; i < root->blocks_cnt; i++) {
        dirent_t *entries = (dirent_t *)&data_blocks[root->blocks[i] * FS_BLOCK_SIZE];
        
        for (int j = 0; j < (FS_BLOCK_SIZE/sizeof(dirent_t)); j++) {
            
            if (entries[j].inode_idx != 0xFFFF) {
                
                // Margem
                if (pos < max_len - 1) buffer[pos++] = ' ';
                if (pos < max_len - 1) buffer[pos++] = ' ';

                // Copia nome
                const char *n = entries[j].name;
                while (*n && pos < max_len - 1) buffer[pos++] = *n++;
                
                // Nova linha
                if (pos < max_len - 1) buffer[pos++] = '\n';
            }
        }
    }
    
    buffer[pos] = 0;
    return 0;

}

// Comando para deletar arquivo
int fs_delete(const char *name) {

    // 1. Busca o arquivo
    int inode_idx = find_inode_by_name(name);
    if (inode_idx < 0) return -1; // Erro: Não existe

    inode_t *inode = &inode_table[inode_idx];

    // 2. Libera os blocos de dados que o arquivo usava
    for (int i = 0; i < inode->blocks_cnt; i++) {
        free_bit(block_bitmap, inode->blocks[i]);
    }

    // 3. Limpa o Inode (Metadados) e libera no bitmap
    inode->size = 0;
    inode->blocks_cnt = 0;
    inode->type = 0;
    free_bit(inode_bitmap, inode_idx);

    // 4. Remove a entrada do Diretório Raiz (Unlink)
    // Precisamos varrer o Inode 0 (Root) para apagar a referência
    inode_t *root = &inode_table[0];
    
    for (int i = 0; i < root->blocks_cnt; i++) {
        // Acessa o bloco de diretório
        dirent_t *entries = (dirent_t *)&data_blocks[root->blocks[i] * FS_BLOCK_SIZE];
        int max_entries = FS_BLOCK_SIZE / sizeof(dirent_t);
        
        for (int j = 0; j < max_entries; j++) {
            // Se encontrar a entrada que aponta para nosso inode...
            if (entries[j].inode_idx == inode_idx) {
                entries[j].inode_idx = 0xFFFF; // Marca como vaga livre
                entries[j].name[0] = 0;        // Limpa o nome
                return 0; // Sucesso!
            }
        }
    }
    
    return -2; // Erro estranho: Inode existia mas não estava listado no diretório

}