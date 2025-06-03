ENTRY(kmain)

SECTIONS
{
  . = 0x100000;               /* load at 1 MiB */
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss :  {
      __bss_start = .;
      *(.bss*)
      *(COMMON)
      __bss_end = .;
  }
}
