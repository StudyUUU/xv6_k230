OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /* * [核心改动] 设置起始虚拟地址 (VMA)
   * 必须匹配 memlayout.h 中的 KERNBASE (0xFFFFFFC000200000)
   */
  . = 0xFFFFFFC000200000;

  /* * [核心策略] 
   * 使用 AT(...) 指令指定加载物理地址 (LMA)。
   * 这样 U-Boot 将 Image 放在 0x200000，但代码内部生成的跳转指令是针对 0xFFFFFFC... 的。
   */

  /* * 优化 1: 代码段 (.text) 单独页对齐
   * 目的: 这一段在 MMU 中将被映射为 "只读 + 可执行 (R|X)"
   */
  .text : AT(0x00200000){
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(etext = .);
  }

  /* * 优化 2: 强制页对齐
   * 目的: 将代码与数据在物理页上彻底隔开。
   * 如果不加这行，.rodata 可能会紧挨着 .text 的尾巴放在同一个 4KB 页里。
   * 导致这个页既需要 "可执行" 又需要 "可读"，破坏了 "W^X" (Write XOR Execute) 安全原则。
   */
  . = ALIGN(0x1000);

  /* * 优化 3: 只读数据段 (.rodata)
   * 目的: 这一段在 MMU 中将被映射为 "只读 (R)"，严禁写入，严禁执行。
   */
  .rodata : {
    . = ALIGN(16);
    /* 合并写法，减少视觉干扰 */
    *(.srodata .srodata.* .rodata .rodata.*)
  }

  /* 再次页对齐，为可写数据做准备 */
  . = ALIGN(0x1000);

  /* * 优化 4: 数据段 (.data)
   * 目的: 这一段在 MMU 中将被映射为 "可读 + 可写 (R|W)"。
   */
  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.* .data .data.*)
  }

  . = ALIGN(16);
  PROVIDE(edata = .);

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.* .bss .bss.*)
  }

  . = ALIGN(16);
  PROVIDE(end = .);

  /* * 优化 5: 丢弃垃圾段
   * 目的: 显式告诉链接器不要把这些非必要的辅助段放进最终镜像。
   */
  /DISCARD/ : {
    *(.eh_frame .note.GNU-stack)
  }
}