# =========================================================
# Toolchain
# =========================================================
TOOLPREFIX = riscv64-unknown-elf-
CC      = $(TOOLPREFIX)gcc
LD      = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy

# =========================================================
# Kernel build config
# =========================================================
K = kernel

KCFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
KCFLAGS += -mcmodel=medany -mno-relax
KCFLAGS += -ffreestanding -fno-common -nostdlib -mno-riscv-attribute
KCFLAGS += -I.

# 核心对象列表
KOBJS = \
  $K/entry.o \
  $K/start.o \
  $K/uart.o \
  $K/printf.o \
  $K/console.o \
  $K/kalloc.o \
  $K/main.o \
  $K/string.o \
  $K/vm.o \
  $K/kernelvec.o \
  $K/trap.o \
  $K/timer.o \
  $K/spinlock.o \
  $K/sleeplock.o \
  $K/proc.o \
  $K/plic.o \
  $K/swtch.o \
  $K/trampoline.o \
  $K/wdt.o \
  $K/syscall.o \
  $K/sysfile.o \
  $K/sysproc.o \
  $K/bio.o \
  $K/fs.o \
  $K/log.o \
  $K/file.o \
  $K/pipe.o \
  $K/ramdisk.o \
  $K/ramdisk_img.o \
  $K/exec.o

# =========================================================
# User / FS build config
# =========================================================
U = user
MKFS = mkfs/mkfs

UCFLAGS = -Wall -O -ffreestanding -nostdlib -mno-relax
UCFLAGS += -march=rv64gc -mabi=lp64

INITCODE_O   = $(U)/initcode.o
INITCODE_ELF = $(U)/initcode
INITCODE_BIN = $(U)/initcode.bin
INITCODE_H   = $(K)/initcode.h

# 将来可以在这里添加更多的用户程序，例如 $U/_init $U/_sh 等
# 目前为了测试，我们把 initcode 这个 ELF 文件也放进文件系统里
UPROGS = \
    $U/initcode

# =========================================================
# Top-level targets
# =========================================================
all: $(INITCODE_H) kernel.bin

# =========================================================
# initcode build rules
# =========================================================
$(INITCODE_O): $(U)/initcode.S
	$(CC) $(UCFLAGS) -c $< -o $@

$(INITCODE_ELF): $(INITCODE_O)
	$(LD) -N -Ttext 0 -o $@ $<

$(INITCODE_BIN): $(INITCODE_ELF)
	$(OBJCOPY) -O binary $< $@

$(INITCODE_H): $(INITCODE_BIN)
	xxd -i $< > $@

# =========================================================
# File System generation rules (New!)
# =========================================================

# 1. 编译 mkfs 工具 (注意：使用宿主机 gcc，不是 riscv gcc)
$(MKFS): mkfs/mkfs.c $K/fs.h $K/types.h $K/stat.h $K/param.h
	gcc -Werror -Wall -I. -o $(MKFS) mkfs/mkfs.c

# 2. 生成 fs.img
# 依赖于 mkfs 工具和所有用户程序
fs.img: $(MKFS) $(UPROGS)
	$(MKFS) fs.img $(UPROGS)

# =========================================================
# kernel build rules
# =========================================================

# 链接内核
kernel.bin: $(KOBJS) $(K)/kernel.ld $(INITCODE_H) fs.img
	$(LD) -T $(K)/kernel.ld -o kernel.elf $(KOBJS)
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	cp kernel.bin /home/alientek/linux/tftp/

# 通用编译规则 (.c -> .o)
$K/%.o: $K/%.c
	$(CC) $(KCFLAGS) -c $< -o $@

# 通用编译规则 (.S -> .o)
# 注意：这里会覆盖 ramdisk_img.S 的规则，所以下面必须显式定义它
$K/%.o: $K/%.S
	$(CC) $(KCFLAGS) -c $< -o $@

# [关键修复] 显式定义 ramdisk_img.o 的规则
# 强制要求先生成 fs.img，再编译这个汇编文件
$K/ramdisk_img.o: $K/ramdisk_img.S fs.img
	$(CC) $(KCFLAGS) -c $< -o $@

# =========================================================
# clean
# =========================================================
clean:
	rm -f \
    $(K)/*.o \
    kernel.elf kernel.bin \
    $(U)/initcode.o \
    $(U)/initcode \
    $(U)/initcode.bin \
    $(INITCODE_H) \
    $(MKFS) \
    fs.img