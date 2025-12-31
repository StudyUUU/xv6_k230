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
  $K/proc.o \
  $K/plic.o \
  $K/swtch.o \
  $K/trampoline.o \
  $K/wdt.o \
  $K/syscall.o \
  $K/sysfile.o \
  $K/sysproc.o

# =========================================================
# User initcode build config
# =========================================================
U = user

UCFLAGS = -Wall -O -ffreestanding -nostdlib -mno-relax
UCFLAGS += -march=rv64gc -mabi=lp64

INITCODE_O   = $(U)/initcode.o
INITCODE_ELF = $(U)/initcode
INITCODE_BIN = $(U)/initcode.bin
INITCODE_H   = $(K)/initcode.h

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
# kernel build rules
# =========================================================
kernel.bin: $(KOBJS) $(K)/kernel.ld $(INITCODE_H)
	$(LD) -T $(K)/kernel.ld -o kernel.elf $(KOBJS)
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	cp kernel.bin /home/alientek/linux/tftp/

$K/%.o: $K/%.c
	$(CC) $(KCFLAGS) -c $< -o $@

$K/%.o: $K/%.S
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
	  $(INITCODE_H)
