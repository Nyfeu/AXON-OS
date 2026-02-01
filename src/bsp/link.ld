OUTPUT_ARCH( "riscv" )
ENTRY( _start )

MEMORY
{
  /* QEMU virt RAM começa em 0x80000000. Vamos dar 256KB */
  ram  (wxa) : ORIGIN = 0x80000000, LENGTH = 256K
}

SECTIONS
{
  
  .text : {
    PROVIDE(_text_start = .);
    *(.text .text.*)
    PROVIDE(_text_end = .);
  } >ram

  .rodata : {
    *(.rodata .rodata.*)
  } >ram

  .data : {
    *(.data .data.*)
  } >ram

  .bss : {
    _bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    _bss_end = .;
  } >ram

  . = ALIGN(4);
  PROVIDE(_end = .);

  /* Define o topo da pilha no final da RAM disponível */
  PROVIDE(_stack_top = ORIGIN(ram) + LENGTH(ram));

}