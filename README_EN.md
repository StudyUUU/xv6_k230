# xv6-k230: RISC-V Operating System Port

<div align="center">

**MIT xv6 Teaching OS Ported to K230 (CanMV-K230) Platform**

[![Platform](https://img.shields.io/badge/Platform-K230-blue.svg)](https://www.canaan-creative.com/)
[![ISA](https://img.shields.io/badge/ISA-RISC--V64-green.svg)](https://riscv.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

English | [简体中文](README.md)

</div>

---

## 📖 Overview

xv6-k230 is a complete port of the [MIT xv6](https://pdos.csail.mit.edu/6.828/2023/xv6.html) teaching operating system to the **K230 chip platform** (CanMV-K230 development board). This project implements a minimal but fully functional Unix-like operating system, demonstrating core OS concepts:

- 🔹 **Virtual Memory Management** (Sv39 paging)
- 🔹 **Process Management** (fork/exec/wait/exit)
- 🔹 **System Call Interface** (21 standard syscalls)
- 🔹 **File System** (inode-based FS with logging)
- 🔹 **Device Drivers** (UART/PLIC/Timer/SD Card/Watchdog)
- 🔹 **Multi-Core Support** (up to 2 CPU cores)

### 🎯 Target Platform

- **Board**: CanMV-K230 (K230 SoC)
- **CPU**: Dual-core RISC-V64 C908 @ 1.6GHz
- **RAM**: ~1GB (1022MB usable, 2MB reserved for OpenSBI)
- **Boot**: U-Boot → TFTP/SD Card → kernel.bin @ 0x00200000
- **Storage**: SD Card (SDHCI interface, GPT partitioning, filesystem at LBA 0xf000)

---

## ✨ Key Features

### 🚀 Implemented Functionality

#### Core Subsystems
| Subsystem | Description |
|-----------|-------------|
| **Memory Management** | 4KB physical page allocator + Sv39 three-level page table + lazy allocation |
| **Process Scheduler** | Simple round-robin scheduling + 64 process slots + user/kernel isolation |
| **Trap Handling** | Trampoline mechanism + interrupt delegation + syscall interface |
| **File System** | 1024-byte blocks + logging layer + inode cache + directory support |
| **Synchronization** | Spinlock (interrupt-disabling) + Sleep lock (sleepable) |

#### Device Drivers
| Device | Base Address | Function |
|--------|--------------|----------|
| **UART0** | 0x91400000 | DW8250 serial port (115200 baud, console I/O) |
| **PLIC** | 0x0f00000000 | Platform-Level Interrupt Controller (requires M-mode unlock) |
| **Timer** | CSR: stimecmp | RISC-V standard timer (time-slice scheduling) |
| **SD Card** | 0x91581000 | SDHCI 3.0 controller (SD 2.0 support, 4-bit bus, 25MHz) |
| **Watchdog** | 0x91106000 | WDT0 (system reboot functionality) |

#### System Calls (21)
```c
fork, exit, wait, pipe, read, write, close, kill, exec, 
open, mknod, unlink, link, mkdir, chdir, dup, getpid, 
sbrk, pause, uptime, fstat
```

#### User Programs
```
init       - Init process (launches shell)
sh         - Interactive shell
ls         - List directory contents
cat        - Display file contents
echo       - Echo text
grep       - Text search
rm         - Remove files
mkdir      - Create directories
usertests  - Comprehensive test suite (40+ test cases)
```

---

## 🏗️ System Architecture

### Boot Sequence

```
┌─────────────────────────────────────────────────────────────┐
│  M-Mode (Machine Mode)                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ U-Boot → TFTP loads kernel.bin → 0x00200000        │   │
│  │ ↓                                                    │   │
│  │ entry.S (_entry) → Setup stack → start()           │   │
│  │ ↓                                                    │   │
│  │ start.c (M-Mode initialization):                    │   │
│  │ • Configure mstatus (MPP=S-mode)                    │   │
│  │ • Delegate exceptions/interrupts (medeleg/mideleg)  │   │
│  │ • Unlock PLIC_CTRL (K230-specific, write 1 to addr) │   │
│  │ • Enable T-Head extensions (MXSTATUS.THEADISAEE)    │   │
│  │ • Configure PMP (full physical memory access)       │   │
│  │ • mret → Switch to S-Mode                           │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  S-Mode (Supervisor Mode)                                    │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ main.c (main function):                              │   │
│  │ ├─ consoleinit()    // Initialize UART console      │   │
│  │ ├─ kinit()          // Physical memory allocator    │   │
│  │ ├─ kvminit()        // Setup kernel page table      │   │
│  │ ├─ procinit()       // Process table initialization │   │
│  │ ├─ plicinit()       // PLIC interrupt controller    │   │
│  │ ├─ binit()          // Buffer cache                 │   │
│  │ ├─ iinit()          // Inode table                  │   │
│  │ ├─ fileinit()       // File table                   │   │
│  │ ├─ sd_init()        // SD card init (mount FS)      │   │
│  │ ├─ userinit()       // Start first user process     │   │
│  │ └─ scheduler()      // Enter scheduling loop        │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  U-Mode (User Mode)                                          │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ /init (PID 1):                                       │   │
│  │ • Open/create console device                        │   │
│  │ • fork() + exec("sh") → Launch shell                │   │
│  │ • wait() for shell exit → Restart shell             │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Memory Layout (Sv39 Virtual Memory)

```
┌───────────────────────────────────────────────────────────────┐
│ Virtual Address Space (512GB, Sv39)                           │
├───────────────────────────────────────────────────────────────┤
│ 0x3ffffff000  TRAMPOLINE (trampoline page, user/kernel switch)│
│ 0x3fffffe000  TRAPFRAME (trap frame, save user registers)    │
│      ...         (per-process kernel stacks, high address)    │
│                                                                │
│      ...         (unmapped region)                             │
│                                                                │
│ 0x0f00000000  PLIC (Platform-Level Interrupt Controller)      │
│ 0x91581000    SD1 Controller (SDHCI, Identity Mapping)        │
│ 0x91400000    UART0 (Serial port, Identity Mapping)           │
│ 0x91106000    WDT0 (Watchdog, Identity Mapping)               │
│      ...         (MMIO device region)                          │
│                                                                │
│ 0x40000000    PHYSTOP (Physical memory end)                   │
│      ...         (Kernel heap, dynamically allocated)         │
│ 0x00200000    KERNBASE (Kernel load address, Identity start) │
│    ├─ text        (Kernel code section)                       │
│    ├─ rodata      (Read-only data)                            │
│    ├─ data        (Initialized data)                          │
│    ├─ bss         (Zero-initialized data)                     │
│    └─ end         (Kernel end address, kalloc start point)    │
└───────────────────────────────────────────────────────────────┘

Physical Memory: 0x00200000 ~ 0x40000000 (1022MB usable)
```

### File System Layout (SD Card)

```
┌──────────────────────────────────────────────────────┐
│ SD Card Physical Layout (GPT Partitioning)           │
├──────────────────────────────────────────────────────┤
│ LBA 0         GPT Header                             │
│ LBA 1~33      GPT Partition Table                    │
│      ...      Other partitions (boot/data etc.)      │
│ LBA 0xf000    xv6 Filesystem Start (61440)           │
│  ├─ Block 0      Boot Block (unused)                 │
│  ├─ Block 1      Super Block (FSMAGIC=0x10203040)    │
│  ├─ Block 2~31   Log Blocks (Write-Ahead Log)        │
│  ├─ Block 32~... Inode Blocks                        │
│  ├─ Block ...    Bitmap (free block bitmap)          │
│  └─ Block ...    Data Blocks (actual file data)      │
└──────────────────────────────────────────────────────┘

Block Size: 1024 bytes
Sector Size: 512 bytes (1 xv6 block = 2 SD sectors)
```

---

## 🛠️ Building & Deployment

### Prerequisites

1. **Toolchain**: RISC-V64 GCC cross-compiler
   ```bash
   # Ensure these tools are in PATH
   riscv64-unknown-elf-gcc
   riscv64-unknown-elf-ld
   riscv64-unknown-elf-objcopy
   riscv64-unknown-elf-objdump
   ```

2. **Additional Tools**:
   - `perl` (generates syscall stubs)
   - `make` (build system)

3. **Hardware Setup**:
   - CanMV-K230 development board
   - SD card (GPT-formatted, xv6 partition at LBA 0xf000)
   - UART serial connection (115200 baud)
   - TFTP server (optional, for network boot)

### Build Steps

```bash
# 1. Clone repository
cd /home/alientek/linux/xv6/xv6-k230

# 2. Clean old build artifacts
make clean

# 3. Build kernel and filesystem image
make
```

**Output Files**:
- `kernel/kernel.bin` - Kernel binary (auto-copied to `/home/alientek/linux/tftp/`)
- `fs.img` - Filesystem image (deploy to SD card LBA 0xf000)

### Deploy to K230

#### Method 1: TFTP Network Boot (Recommended for Development)

```bash
# Kernel is automatically copied to TFTP directory
# In U-Boot, execute:
# => dhcp
# => tftpboot 0x00200000 kernel.bin
# => go 0x00200000
```

#### Method 2: SD Card Boot

```bash
# 1. Write kernel.bin to SD boot partition
# 2. Write fs.img to xv6 filesystem partition (LBA 0xf000)
dd if=fs.img of=/dev/sdX bs=512 seek=61440 conv=fsync
```

### Serial Connection

```bash
# Use minicom or screen to connect
screen /dev/ttyUSB0 115200

# Or
minicom -D /dev/ttyUSB0 -b 115200
```

**Successful Boot Output**:
```
xv6 kernel is booting

sd: card initialized, type=SD2.0, RCA=0x0001
sd: mounted xv6 filesystem at LBA 0xf000
virtio_disk_init
hart 0 starting
init: starting sh
welcome to xv6-k230
$ 
```

---

## 🧪 Testing & Validation

### Run Test Suite

```bash
$ usertests
```

**Test Coverage**:
- ✅ **Memory Tests**: `copyinstr`, `copyin`, `copyout`, `sbrkfail`, `bigargtest`, `argptest`
- ✅ **Process Tests**: `forkfork`, `forkforkfork`, `exectest`, `exitwait`, `reparent`
- ✅ **Filesystem**: `createdelete`, `dirfile`, `iref`, `fsfull`, `bigfile`
- ✅ **Pipe Tests**: `pipe1`, `preempt`, `exitiput`
- ✅ **Address Space**: K230-specific MMIO address protection tests (UART/PLIC access blocking)

**Current Status**: 🎉 ALL TESTS PASSED

### Common Command Examples

```bash
# File operations
$ ls           # List current directory
$ cat README   # View README file
$ mkdir test   # Create directory
$ rm oldfile   # Delete file

# Text processing
$ echo hello xv6 > file.txt
$ grep xv6 file.txt

# Background jobs
$ sh &         # Run shell in background
$ sleep 100 &  # Sleep in background
```

---

## 📁 Code Structure

```
xv6-k230/
├── Makefile              # Main build file
├── README.md             # Documentation (Chinese)
├── README_EN.md          # Documentation (English)
│
├── kernel/               # Kernel source
│   ├── entry.S           # Kernel entry (M-Mode boot)
│   ├── start.c           # M-Mode initialization (switch to S-Mode)
│   ├── main.c            # S-Mode main (subsystem init)
│   ├── vm.c / vm.h       # Virtual memory management
│   ├── kalloc.c          # Physical memory allocator
│   ├── proc.c / proc.h   # Process management
│   ├── trap.c            # Trap handling
│   ├── trampoline.S      # User/kernel mode switching
│   ├── syscall.c/.h      # System call dispatch
│   ├── sysfile.c         # File-related syscalls
│   ├── sysproc.c         # Process-related syscalls
│   ├── fs.c / fs.h       # Filesystem core
│   ├── bio.c / buf.h     # Block device buffer layer
│   ├── log.c             # Logging layer (transaction support)
│   ├── file.c / file.h   # File descriptor layer
│   ├── pipe.c            # Pipe implementation
│   ├── exec.c            # ELF loader
│   ├── uart.c            # UART driver (DW8250)
│   ├── plic.c            # PLIC interrupt controller
│   ├── timer.c           # RISC-V timer
│   ├── k230_sd.c/.h      # SD card driver (SDHCI 3.0)
│   ├── wdt.c             # Watchdog driver
│   ├── spinlock.c/.h     # Spinlock
│   ├── sleeplock.c/.h    # Sleep lock
│   ├── string.c          # Kernel string functions
│   └── memlayout.h       # Memory layout definitions
│
├── user/                 # User programs
│   ├── init.c            # Init process (PID 1)
│   ├── sh.c              # Shell
│   ├── ls.c              # ls command
│   ├── cat.c             # cat command
│   ├── echo.c            # echo command
│   ├── grep.c            # grep command
│   ├── rm.c              # rm command
│   ├── mkdir.c           # mkdir command
│   ├── usertests.c       # Test suite
│   ├── ulib.c            # User library
│   ├── umalloc.c         # User-space malloc
│   ├── printf.c          # User-space printf
│   ├── usys.pl           # Syscall stub generator
│   └── user.h            # User-space header
│
└── mkfs/                 # Filesystem build tool
    └── mkfs.c            # Creates fs.img image
```

---

## 🔧 Developer Guide

### Adding a System Call

1. **Define syscall number** ([kernel/syscall.h](kernel/syscall.h))
   ```c
   #define SYS_mynewcall 22
   ```

2. **Declare and register kernel function** ([kernel/syscall.c](kernel/syscall.c))
   ```c
   extern uint64 sys_mynewcall(void);
   
   static uint64 (*syscalls[])(void) = {
       // ...
       [SYS_mynewcall] sys_mynewcall,
   };
   ```

3. **Implement syscall** ([kernel/sysproc.c](kernel/sysproc.c) or [kernel/sysfile.c](kernel/sysfile.c))
   ```c
   uint64 sys_mynewcall(void) {
       int arg1;
       if(argint(0, &arg1) < 0)
           return -1;
       // ... implementation logic
       return 0;
   }
   ```

4. **Add user-space interface** ([user/usys.pl](user/usys.pl))
   ```perl
   entry("mynewcall");
   ```

5. **Declare user function** ([user/user.h](user/user.h))
   ```c
   int mynewcall(int arg1);
   ```

### Adding a User Program

1. **Create source file** `user/myapp.c`
   ```c
   #include "kernel/types.h"
   #include "user/user.h"
   
   int main(int argc, char *argv[]) {
       printf("Hello from myapp\n");
       exit(0);
   }
   ```

2. **Modify Makefile** - Add to `UPROGS`
   ```makefile
   UPROGS=\
       # ...
       $U/_myapp
   ```

3. **Rebuild**
   ```bash
   make
   ```

### Debugging Tips

#### Serial Logging
```c
// kernel/defs.h provides printf
printf("debug: value=%d\n", x);
```

#### Kernel Panic
```c
// Trigger kernel panic and print stack trace
panic("something went wrong");
```

#### GDB Debugging (JTAG)
```bash
# Requires hardware JTAG connection
riscv64-unknown-elf-gdb kernel/kernel
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

#### View Register State
```c
// Print trapframe in trap.c or elsewhere
printf("epc=%p ra=%p sp=%p\n", p->trapframe->epc, 
       p->trapframe->ra, p->trapframe->sp);
```

---

## 🐛 Known Limitations

### Hardware Limitations
- **SD Card Performance**: Uses PIO mode (no DMA), limited throughput
- **No Interrupt I/O**: SD card operations use polling, blocking CPU
- **Fixed Partition**: xv6 filesystem location hardcoded at LBA 0xf000

### System Limitations
- **Process Count**: Maximum 64 processes (NPROC=64)
- **Open Files**: 16 per process (NOFILE=16), 100 system-wide (NFILE=100)
- **File Size**: Maximum 12+256 = 268 blocks = 268KB (MAXFILE limit)
- **Filesystem**: Fixed 2000 blocks (FSSIZE=2000, ~2MB)

### K230-Specific Notes
- **PLIC Unlock**: Must write 1 to `PLIC_CTRL` (0x0f01ffffc) in M-Mode
- **T-Head Extensions**: Requires `MXSTATUS.THEADISAEE` bit for MMU support
- **Timer**: Uses `stimecmp` CSR (not CLINT), requires MENVCFG.STCE=1
- **Dual-Core**: Code supports 2 cores, but default scheduling is simple round-robin

### Memory Test Adjustments
- `sbrkfail`: Adjusted allocation limits for 1GB RAM (original xv6: 128MB)
- `copyin`/`copyout`: Added K230-specific MMIO address protection tests

---

## 📚 References

### Official Documentation
- [xv6 Book (RISC-V Edition)](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf) - MIT teaching manual
- [RISC-V Privileged Specification](https://riscv.org/specifications/) - Architecture specification
- [RISC-V Assembly Reference](https://github.com/riscv-non-isa/riscv-asm-manual/blob/master/riscv-asm.md)

### K230 Platform
- CanMV-K230 Datasheet (Chinese)
- [K230 SDK Documentation](https://github.com/kendryte/k230_sdk)
- C908 Core Manual (T-Head Semiconductor)

### Related Projects
- [MIT xv6 Official Repository](https://github.com/mit-pdos/xv6-riscv)
- [xv6-riscv Course Website](https://pdos.csail.mit.edu/6.828/2023/index.html)

---

## 🤝 Contributing

Issues and Pull Requests are welcome!

### Development Workflow
1. Fork this repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add some amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open Pull Request

### Code Standards
- Follow xv6 code style (K&R indentation, 2 spaces)
- Add necessary comments (especially for K230-specific hardware operations)
- Ensure tests pass with `usertests`

---

## 📄 License

This project is open-sourced under the MIT License, inherited from the original xv6 project.

```
Copyright (c) 2006-2023 Frans Kaashoek, Robert Morris, Russ Cox,
                        Massachusetts Institute of Technology

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files...
```

---

## 🙏 Acknowledgments

- **MIT PDOS Lab** - Original xv6 operating system
- **Canaan Creative** - K230 development board support
- **RISC-V Foundation** - Open ISA
- **T-Head Semiconductor** - C908 core documentation

---

<div align="center">

**⭐ If this project helps you, please give it a Star! ⭐**

Made with ❤️ by xv6-k230 Contributors

</div>
