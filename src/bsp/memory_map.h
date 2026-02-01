#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

// Endereços físicos da máquina 'virt' do QEMU
#define UART0_BASE    0x10000000
#define CLINT_BASE    0x02000000
#define PLIC_BASE     0x0c000000

// Offsets do CLINT (Core Local Interruptor)
// mtimecmp: Registrador de comparação (gera interrupção quando mtime >= mtimecmp)
// mtime:    Contador de tempo real (64 bits)
#define CLINT_MTIMECMP(hart) (CLINT_BASE + 0x4000 + 8 * (hart))
#define CLINT_MTIME          (CLINT_BASE + 0xBFF8)

// Macros de Acesso
#define MMIO8(addr)   (*(volatile unsigned char *)(addr))
#define MMIO32(addr)  (*(volatile unsigned int *)(addr))
#define MMIO64(addr)  (*(volatile unsigned long long *)(addr))

#endif