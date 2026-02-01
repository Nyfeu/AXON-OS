#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <stdint.h>

// Macros de Acesso Genéricos
#define MMIO8(addr)   (*(volatile uint8_t  *)(addr))
#define MMIO32(addr)  (*(volatile uint32_t *)(addr))
#define MMIO64(addr)  (*(volatile uint64_t *)(addr))

#ifdef PLATFORM_FPGA

    /* ========================================================== */
    /* [FPGA] AXON-SOC-RV32I - Endereços Reais                    */
    /* ========================================================== */

    #define UART0_BASE           0x10000000
    #define CLINT_BASE           0x50000000
    #define PLIC_BASE            0x60000000
    
    // Offsets da sua UART Customizada
    #define UART_REG_DATA        (UART0_BASE + 0x00)
    #define UART_REG_CTRL        (UART0_BASE + 0x04)
    
    // Offsets do CLINT Customizado
    #define CLINT_MTIMECMP(hart) (CLINT_BASE + 0x08) 
    #define CLINT_MTIME          (CLINT_BASE + 0x10)

#else

    /* ========================================================== */
    /* [SIM] QEMU 'VIRT' MACHINE                                  */
    /* ========================================================== */

    #define UART0_BASE           0x10000000
    #define CLINT_BASE           0x02000000
    #define PLIC_BASE            0x0c000000

    // Offsets Padrão RISC-V (QEMU 16550 UART)
    #define CLINT_MTIMECMP(hart) (CLINT_BASE + 0x4000 + 8 * (hart))
    #define CLINT_MTIME          (CLINT_BASE + 0xBFF8)

#endif

#endif