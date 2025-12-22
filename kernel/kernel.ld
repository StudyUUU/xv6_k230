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
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
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

  /* 必须定义 edata，否则 entry.S 汇编无法找到符号 */
  . = ALIGN(16);
  PROVIDE(edata = .);

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) 
    . = ALIGN(16);
    *(.bss .bss.*)
  }

  PROVIDE(end = .);
}