#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <stdint.h>

// Macros de Acesso
#define MMIO8(addr)   (*(volatile uint8_t  *)(addr))
#define MMIO32(addr)  (*(volatile uint32_t *)(addr))
#define MMIO64(addr)  (*(volatile uint64_t *)(addr))

/* ========================================================== */
/* [SIM] QEMU 'VIRT' MACHINE ADDRESSES                        */
/* ========================================================== */

#define UART0_BASE      0x10000000
#define CLINT_BASE      0x02000000
#define PLIC_BASE       0x0c000000

// Endereços Fictícios (Stubs) para periféricos que não existem no QEMU
#define NPU_BASE        0x90000000 
#define DMA_BASE        0x40000000 
#define VGA_BASE        0x30000000

/* --- CLINT OFFSETS (QEMU / SiFive Standard) --- */
// Diferente da FPGA, o QEMU usa o layout SiFive padrão

#define CLINT_MSIP          MMIO32(CLINT_BASE + 0x0000)
#define CLINT_MTIMECMP_LO   MMIO32(CLINT_BASE + 0x4000)
#define CLINT_MTIMECMP_HI   MMIO32(CLINT_BASE + 0x4004)
#define CLINT_MTIME_LO      MMIO32(CLINT_BASE + 0xBFF8)
#define CLINT_MTIME_HI      MMIO32(CLINT_BASE + 0xBFFC)

/* --- PLIC OFFSETS (Standard RISC-V) --- */
// Macros auxiliares para o driver hal_plic.c

#define PLIC_PRIORITY_BASE  (PLIC_BASE + 0x000000)
#define PLIC_PENDING_BASE   (PLIC_BASE + 0x001000)
#define PLIC_ENABLE_BASE    (PLIC_BASE + 0x002000)

#define PLIC_THRESHOLD      MMIO32(PLIC_BASE + 0x200000)
#define PLIC_CLAIM          MMIO32(PLIC_BASE + 0x200004)

#define PLIC_PRIORITY(id)   MMIO32(PLIC_PRIORITY_BASE + ((id) * 4))
#define PLIC_PENDING        MMIO32(PLIC_PENDING_BASE)
#define PLIC_ENABLE         MMIO32(PLIC_ENABLE_BASE)

/* --- UART 16550 REGISTERS --- */
// A UART do QEMU é uma NS16550A padrão (8 bits, offsets padrão)

#define UART_RBR  0x00 // Receiver Buffer (Read)
#define UART_THR  0x00 // Transmitter Holding (Write)
#define UART_IER  0x01 // Interrupt Enable
#define UART_FCR  0x02 // FIFO Control
#define UART_LCR  0x03 // Line Control
#define UART_LSR  0x05 // Line Status

#endif