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

KOBJS = \
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
UCFLAGS += -I.

LDFLAGS = -z max-page-size=4096

INITCODE_O   = $(U)/initcode.o
INITCODE_ELF = $(U)/initcode
INITCODE_BIN = $(U)/initcode.bin
INITCODE_H   = $(K)/initcode.h

# 用户态基础库对象
ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o

# 文件系统中的用户程序列表
UPROGS=\
    $U/initcode\
    $U/_init\
    $U/_sh\

# =========================================================
# Top-level targets
# =========================================================
all: $(INITCODE_H) kernel.bin

# =========================================================
# User Library & Program Rules (新增重点)
# =========================================================

# 1. 生成系统调用汇编存根 (依赖 perl 脚本)
$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S

# 2. 编译 usys.S
$U/usys.o : $U/usys.S
	$(CC) $(UCFLAGS) -c -o $U/usys.o $U/usys.S

# 3. 核心链接规则：将 user/*.c 链接成 user/_* 二进制程序
# 解决了 "No rule to make target 'user/_init'" 问题
$U/_%: $U/%.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJCOPY) -S -O binary $@ $@

# 4. 用户态 C 文件编译规则
$U/%.o: $U/%.c
	$(CC) $(UCFLAGS) -c -o $@ $<

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
# File System generation rules
# =========================================================

$(MKFS): mkfs/mkfs.c $K/fs.h $K/types.h $K/stat.h $K/param.h
	gcc -Werror -Wall -I. -o $(MKFS) mkfs/mkfs.c

fs.img: $(MKFS) README $(UPROGS)
	$(MKFS) fs.img README $(UPROGS)

# =========================================================
# kernel build rules
# =========================================================

kernel.bin: $(KOBJS) $(K)/kernel.ld $(INITCODE_H) fs.img
	$(LD) -T $(K)/kernel.ld -o kernel.elf $(KOBJS)
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	cp kernel.bin /home/alientek/linux/tftp/

$K/%.o: $K/%.c
	$(CC) $(KCFLAGS) -c $< -o $@

$K/%.o: $K/%.S
	$(CC) $(KCFLAGS) -c $< -o $@

$K/ramdisk_img.o: $K/ramdisk_img.S fs.img
	$(CC) $(KCFLAGS) -c $< -o $@

# =========================================================
# clean
# =========================================================
clean:
	rm -f \
    $(K)/*.o \
    $(U)/*.o \
    $(U)/_*\
    $(U)/usys.S \
    kernel.elf kernel.bin \
    $(U)/initcode.o \
    $(U)/initcode \
    $(U)/initcode.bin \
    $(INITCODE_H) \
    $(MKFS) \
    fs.img