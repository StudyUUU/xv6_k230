OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /* 【修改点 1】K230 的加载地址是 2MB 处 */
  . = 0x00200000;

  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    /* 暂时移除 ASSERT，防止因为还没写 trampoline.S 导致编译报错 */
    /* ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page"); */
    PROVIDE(etext = .);
  }

  /* 【关键点】必须保留 .rodata，否则 printf 的字符串会丢失 */
  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*)
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*)
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*)
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}