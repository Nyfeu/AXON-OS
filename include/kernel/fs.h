#ifndef FS_H
#define FS_H

#include <stdint.h>

// ============================================================================
// CONFIGURAÇÕES DO SISTEMA DE ARQUIVOS (Mini-Ext2)
// ============================================================================

#define FS_BLOCK_SIZE      256   // Blocos pequenos (256 bytes) para economizar RAM
#define FS_MAX_INODES      32    // Máximo de 32 arquivos
#define FS_MAX_BLOCKS      128   // Máximo de 128 blocos de dados (32KB total)
#define FS_MAGIC           0xEF53 // Assinatura mágica (Ext2 signature)

// ============================================================================
// ESTRUTURAS DE DISCO
// ============================================================================

// 1. O INODE (Index Node)
// Guarda TUDO sobre o arquivo, MENOS o nome.
typedef struct {
    uint32_t size;       // Tamanho do arquivo em bytes
    uint16_t type;       // 0 = Livre, 1 = Arquivo, 2 = Diretório
    uint16_t blocks_cnt; // Quantos blocos esse arquivo usa
    
    // Ponteiros diretos para os blocos de dados.
    // Simples: Suporta arquivos de até 6 * 256 = 1.5KB
    // (Para arquivos maiores, precisaríamos de blocos indiretos)
    uint16_t blocks[6];  
} inode_t;

// 2. O DIRETÓRIO (Directory Entry)
// Liga um NOME a um INODE.
#define FS_MAX_NAME 28

typedef struct {
    uint16_t inode_idx;      // Qual inode descreve este arquivo?
    char     name[FS_MAX_NAME]; // O nome "humano" do arquivo
} dirent_t;

// 3. O SUPERBLOCO
// O cabeçalho geral da partição.
typedef struct {
    uint16_t magic;          // Assinatura (0xEF53)
    uint16_t inode_count;    // Total de inodes (32)
    uint16_t block_count;    // Total de blocos (128)
    uint16_t free_inodes;    // Inodes livres
    uint16_t free_blocks;    // Blocos livres
} superblock_t;

// ============================================================================
// API DO KERNEL
// ============================================================================

void fs_init(void);
void fs_format(void); // Zera tudo e cria o sistema limpo

// Operações de Arquivo
int fs_create(const char *name);
int fs_write(const char *name, const uint8_t *data, uint32_t len);
int fs_read(const char *name, uint8_t *buffer, uint32_t max_len);
int fs_delete(const char *name);
int fs_list(char *buffer, uint32_t max_len);

// Debug
void fs_debug(void);

#endif