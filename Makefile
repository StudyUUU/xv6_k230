TOOLPREFIX = riscv64-unknown-elf-
CC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy

# 编译选项：禁止标准库，禁止浮点，指定架构
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -mcmodel=medany -mno-relax
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-riscv-attribute

OBJS = entry.o start.o uart.o main.o

all: kernel.bin

# 链接步骤
kernel.bin: $(OBJS) kernel.ld
	$(LD) -T kernel.ld -o kernel.elf $(OBJS)
	$(OBJCOPY) -O binary kernel.elf kernel.bin
	cp kernel.bin /home/alientek/linux/tftp/
# 编译 C 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 编译汇编文件
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf *.bin