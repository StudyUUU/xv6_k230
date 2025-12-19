TOOLPREFIX = riscv64-unknown-elf-
CC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy

# 编译器参数：增加了 -I. 以便能找到头文件
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -mcmodel=medany -mno-relax
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-riscv-attribute
CFLAGS += -I. 

# 源码都在 kernel/ 目录下
K = kernel

# 定义源文件列表
OBJS = \
  $K/entry.o \
  $K/start.o \
  $K/uart.o \
  $K/kalloc.o \
  $K/main.o \
  $K/string.o \
  $K/vm.o \
  $K/kernelvec.o \
  $K/trap.o \
  $K/timer.o \
  $K/spinlock.o \
  $K/proc.o \
  # 将来在这里添加 $K/vm.o $K/proc.o ...

# 最终目标
all: kernel.bin

# 链接规则：注意依赖 kernel/kernel.ld
kernel.bin: $(OBJS) $K/kernel.ld
	$(LD) -T $K/kernel.ld -o kernel.elf $(OBJS)
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	cp kernel.bin /home/alientek/linux/tftp/

# 通用编译规则：自动匹配 kernel/ 下的 .c 和 .S
$K/%.o: $K/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$K/%.o: $K/%.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $K/*.o *.elf *.bin