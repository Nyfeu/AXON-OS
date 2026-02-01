.section .text
.global _start

_start:
    # 1. Stack pointer
    la sp, _stack_top

    # 2. Zerar BSS
    la t0, _bss_start
    la t1, _bss_end

bss_loop:
    bge t0, t1, bss_done
    sw zero, 0(t0)
    addi t0, t0, 4
    j bss_loop

bss_done:
    # 3. Entrar no kernel C
    call kernel_main

# 4. Segurança se retornar
hang:
    wfi
    j hang
