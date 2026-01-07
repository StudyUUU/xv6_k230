# =========================================================
# Toolchain & Tools
# =========================================================
TOOLPREFIX = riscv64-unknown-elf-
CC      = $(TOOLPREFIX)gcc
LD      = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

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

# 用户态基础库对象
ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o

# 文件系统中的用户程序列表 (移除了 initcode，增加了 ls, cat 等常用工具)
UPROGS=\
    $U/_init\
    $U/_sh\
    $U/_ls\
    $U/_cat\
    $U/_echo\
    $U/_grep\
    $U/_rm\
    $U/_mkdir\
    $U/_schedtest\
    $U/_usertests

# =========================================================
# Top-level targets
# =========================================================
# 不再依赖 $(INITCODE_H)
all: kernel.bin

# =========================================================
# User Library & Program Rules
# =========================================================

$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S

$U/usys.o : $U/usys.S
	$(CC) $(UCFLAGS) -c -o $U/usys.o $U/usys.S

# 核心链接规则：保持为 ELF 格式
$U/_%: $U/%.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $U/$*.asm

$U/%.o: $U/%.c
	$(CC) $(UCFLAGS) -c -o $@ $<

# =========================================================
# File System generation rules
# =========================================================

$(MKFS): mkfs/mkfs.c $K/fs.h $K/types.h $K/stat.h $K/param.h
	gcc -Werror -Wall -I. -o $(MKFS) mkfs/mkfs.c

# 确保 README 存在，否则 mkfs 会报错
fs.img: $(MKFS) README $(UPROGS)
	$(MKFS) fs.img README $(UPROGS)

# =========================================================
# kernel build rules
# =========================================================

# 去掉了 $(INITCODE_H) 依赖
kernel.bin: $(KOBJS) $(K)/kernel.ld fs.img
	$(LD) -T $(K)/kernel.ld -o kernel.elf $(KOBJS)
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	cp kernel.bin /home/alientek/linux/tftp/

$K/%.o: $K/%.c
	$(CC) $(KCFLAGS) -c $< -o $@

$K/%.o: $K/%.S
	$(CC) $(KCFLAGS) -c $< -o $@

# ramdisk 包含 fs.img
$K/ramdisk_img.o: $K/ramdisk_img.S fs.img
	$(CC) $(KCFLAGS) -c $< -o $@

# =========================================================
# clean
# =========================================================
clean:
	rm -f \
	$K/*.o $U/*.o $U/_* $U/*.asm $U/usys.S \
	kernel.elf kernel.bin fs.img $(MKFS)