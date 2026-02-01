OUTPUT_ARCH( "riscv" )
ENTRY( _start )

MEMORY
{
  /* * [FPGA CONFIG]
   * ORIGIN: 0x80000800 (Pula 2KB do Bootloader)
   * LENGTH: 254KB (256KB Total - 2KB reservados)
   */
  ram  (wxa) : ORIGIN = 0x80000800, LENGTH = 254K
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

  /* Stack no final absoluto da RAM física (0x80040000) */
  PROVIDE(_stack_top = ORIGIN(ram) + LENGTH(ram));
}