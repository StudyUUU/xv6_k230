# xv6-k230: RISC-V操作系统移植项目

<div align="center">

**MIT xv6教学操作系统在K230 (CanMV-K230)平台上的移植版本**

[![Platform](https://img.shields.io/badge/Platform-K230-blue.svg)](https://www.canaan-creative.com/)
[![ISA](https://img.shields.io/badge/ISA-RISC--V64-green.svg)](https://riscv.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

[English](README_EN.md) | 简体中文

</div>

---

## 📖 项目简介

xv6-k230是[MIT xv6](https://pdos.csail.mit.edu/6.828/2023/xv6.html)教学操作系统在**K230芯片平台**（CanMV-K230开发板）上的完整移植。该项目实现了一个精简但功能完整的类Unix操作系统，展示了操作系统的核心概念：

- 🔹 **虚拟内存管理** (Sv39 分页机制)
- 🔹 **进程管理** (fork/exec/wait/exit)
- 🔹 **系统调用接口** (21个标准系统调用)
- 🔹 **文件系统** (inode-based FS with logging)
- 🔹 **设备驱动** (UART/PLIC/Timer/SD卡/看门狗)
- 🔹 **多核支持** (最多2个CPU核心)

### 🎯 目标平台

- **开发板**: CanMV-K230 (K230芯片)
- **处理器**: 双核 RISC-V64 C908 @ 1.6GHz
- **内存**: ~1GB RAM (1022MB可用, 2MB预留给OpenSBI)
- **启动方式**: U-Boot → TFTP/SD卡 → kernel.bin @ 0x00200000
- **存储**: SD卡 (SDHCI接口, 使用GPT分区, 文件系统位于LBA 0xf000)

---

## ✨ 主要特性

### 🚀 已实现功能

#### 核心子系统
| 子系统 | 说明 |
|--------|------|
| **内存管理** | 4KB物理页分配器 + Sv39三级页表 + 延迟分配支持 |
| **进程调度** | 简单轮转调度 + 64个进程槽位 + 用户/内核态隔离 |
| **陷阱处理** | Trampoline机制 + 中断委托 + 系统调用接口 |
| **文件系统** | 1024字节块大小 + 日志层事务 + inode缓存 + 目录支持 |
| **同步原语** | Spinlock (关中断) + Sleep lock (可睡眠) |

#### 设备驱动
| 设备 | 基地址 | 功能 |
|------|--------|------|
| **UART0** | 0x91400000 | DW8250串口 (115200波特率, 控制台I/O) |
| **PLIC** | 0x0f00000000 | 平台级中断控制器 (需M-mode解锁) |
| **Timer** | CSR: stimecmp | RISC-V标准定时器 (时间片调度) |
| **SD卡** | 0x91581000 | SDHCI 3.0控制器 (支持SD 2.0, 4位总线, 25MHz) |
| **看门狗** | 0x91106000 | WDT0 (系统重启功能) |

#### 系统调用 (21个)
```c
fork, exit, wait, pipe, read, write, close, kill, exec, 
open, mknod, unlink, link, mkdir, chdir, dup, getpid, 
sbrk, pause, uptime, fstat
```

#### 用户程序
```
init  - 初始化程序 (启动shell)
sh    - Shell交互式命令行
ls    - 列出目录内容
cat   - 显示文件内容
echo  - 输出文本
grep  - 文本搜索
rm    - 删除文件
mkdir - 创建目录
usertests - 综合测试套件 (40+测试用例)
```

---

## 🏗️ 系统架构

### 启动流程

```
┌─────────────────────────────────────────────────────────────┐
│  M-Mode (Machine Mode)                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ U-Boot → TFTP加载 kernel.bin → 0x00200000          │   │
│  │ ↓                                                    │   │
│  │ entry.S (_entry) → 设置栈指针 → start()           │   │
│  │ ↓                                                    │   │
│  │ start.c (M-Mode初始化):                            │   │
│  │ • 配置mstatus (MPP=S-mode)                         │   │
│  │ • 异常/中断委托 (medeleg/mideleg)                 │   │
│  │ • 解锁PLIC_CTRL (K230特有, 写1到0x0f01ffffc)      │   │
│  │ • 使能T-Head扩展 (MXSTATUS.THEADISAEE)            │   │
│  │ • 配置PMP (全物理内存访问)                         │   │
│  │ • mret → 切换到S-Mode                              │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  S-Mode (Supervisor Mode)                                    │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ main.c (main函数):                                  │   │
│  │ ├─ consoleinit()    // 初始化UART控制台            │   │
│  │ ├─ kinit()          // 物理内存分配器              │   │
│  │ ├─ kvminit()        // 建立内核页表                │   │
│  │ ├─ procinit()       // 进程表初始化                │   │
│  │ ├─ plicinit()       // PLIC中断控制器              │   │
│  │ ├─ binit()          // 缓冲区缓存                  │   │
│  │ ├─ iinit()          // inode表                     │   │
│  │ ├─ fileinit()       // 文件表                      │   │
│  │ ├─ sd_init()        // SD卡初始化 (挂载文件系统)   │   │
│  │ ├─ userinit()       // 启动第一个用户进程 (init)   │   │
│  │ └─ scheduler()      // 进入调度循环                │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  U-Mode (User Mode)                                          │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ /init (PID 1):                                       │   │
│  │ • 打开/创建console设备                              │   │
│  │ • fork() + exec("sh") → 启动Shell                  │   │
│  │ • wait()等待shell退出 → 重启shell                  │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 内存布局 (Sv39虚拟内存)

```
┌───────────────────────────────────────────────────────────────┐
│ 虚拟地址空间 (512GB, Sv39)                                     │
├───────────────────────────────────────────────────────────────┤
│ 0x3ffffff000  TRAMPOLINE (跳板页, 用户/内核态切换)           │
│ 0x3fffffe000  TRAPFRAME (陷阱帧, 保存用户寄存器)             │
│      ...         (每进程内核栈, 高地址映射)                   │
│                                                                │
│      ...         (未映射区域)                                  │
│                                                                │
│ 0x0f00000000  PLIC (平台级中断控制器, Identity Mapping)       │
│ 0x91581000    SD1 控制器 (SDHCI, Identity Mapping)            │
│ 0x91400000    UART0 (串口, Identity Mapping)                  │
│ 0x91106000    WDT0 (看门狗, Identity Mapping)                 │
│      ...         (MMIO设备区)                                 │
│                                                                │
│ 0x40000000    PHYSTOP (物理内存结束)                          │
│      ...         (内核堆, 动态分配)                           │
│ 0x00200000    KERNBASE (内核加载地址, Identity Mapping起点)  │
│    ├─ text        (内核代码段)                                │
│    ├─ rodata      (只读数据)                                  │
│    ├─ data        (已初始化数据)                              │
│    ├─ bss         (零初始化数据)                              │
│    └─ end         (内核结束地址, kalloc起点)                  │
└───────────────────────────────────────────────────────────────┘

物理内存: 0x00200000 ~ 0x40000000 (1022MB可用)
```

### 文件系统布局 (SD卡)

```
┌──────────────────────────────────────────────────────┐
│ SD卡物理布局 (GPT分区)                                │
├──────────────────────────────────────────────────────┤
│ LBA 0         GPT Header                             │
│ LBA 1~33      GPT Partition Table                    │
│      ...      其他分区 (boot/data等)                 │
│ LBA 0xf000    xv6 文件系统起始 (61440)               │
│  ├─ Block 0      Boot Block (未使用)                 │
│  ├─ Block 1      Super Block (FSMAGIC=0x10203040)    │
│  ├─ Block 2~31   Log Blocks (Write-Ahead Log)        │
│  ├─ Block 32~... Inode Blocks                        │
│  ├─ Block ...    Bitmap (空闲块位图)                 │
│  └─ Block ...    Data Blocks (实际文件数据)          │
└──────────────────────────────────────────────────────┘

块大小: 1024字节
扇区大小: 512字节 (1个xv6块 = 2个SD扇区)
```

---

## 🛠️ 构建与部署

### 前置依赖

1. **工具链**: RISC-V64 GCC交叉编译工具链
   ```bash
   # 确保以下工具在PATH中
   riscv64-unknown-elf-gcc
   riscv64-unknown-elf-ld
   riscv64-unknown-elf-objcopy
   riscv64-unknown-elf-objdump
   ```

2. **其他工具**:
   - `perl` (生成系统调用存根)
   - `make` (构建系统)

3. **硬件环境**:
   - CanMV-K230开发板
   - SD卡 (已格式化为GPT, xv6分区位于LBA 0xf000)
   - UART串口连接 (115200波特率)
   - TFTP服务器 (可选, 用于网络启动)

### 编译步骤

```bash
# 1. 克隆仓库
cd /home/alientek/linux/xv6/xv6-k230

# 2. 清理旧构建产物
make clean

# 3. 编译内核和文件系统镜像
make
```

**输出文件**:
- `kernel/kernel.bin` - 内核二进制文件 (自动复制到 `/home/alientek/linux/tftp/`)
- `fs.img` - 文件系统镜像 (需部署到SD卡LBA 0xf000)

### 部署到K230

#### 方法1: TFTP网络启动 (推荐开发调试)

```bash
# 内核已自动复制到TFTP目录
# 在U-Boot中执行:
# => dhcp
# => tftpboot 0x00200000 kernel.bin
# => go 0x00200000
```

#### 方法2: SD卡启动

```bash
# 1. 将kernel.bin写入SD卡启动分区
# 2. 将fs.img写入xv6文件系统分区 (LBA 0xf000)
dd if=fs.img of=/dev/sdX bs=512 seek=61440 conv=fsync
```

### 串口连接

```bash
# 使用minicom或screen连接串口
screen /dev/ttyUSB0 115200

# 或者
minicom -D /dev/ttyUSB0 -b 115200
```

**启动成功输出**:
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

## 🧪 测试与验证

### 运行测试套件

```bash
$ usertests
```

**测试覆盖**:
- ✅ **内存测试**: `copyinstr`, `copyin`, `copyout`, `sbrkfail`, `bigargtest`, `argptest`
- ✅ **进程测试**: `forkfork`, `forkforkfork`, `exectest`, `exitwait`, `reparent`
- ✅ **文件系统**: `createdelete`, `dirfile`, `iref`, `fsfull`, `bigfile`
- ✅ **管道测试**: `pipe1`, `preempt`, `exitiput`
- ✅ **地址空间**: K230特定MMIO地址保护测试 (UART/PLIC访问拦截)

**当前状态**: 🎉 所有测试通过 (ALL TESTS PASSED)

### 常用命令示例

```bash
# 文件操作
$ ls           # 列出当前目录
$ cat README   # 查看README文件
$ mkdir test   # 创建目录
$ rm oldfile   # 删除文件

# 文本处理
$ echo hello xv6 > file.txt
$ grep xv6 file.txt

# 后台任务
$ sh &         # 后台运行shell
$ sleep 100 &  # 后台睡眠
```

---

## 📁 代码结构

```
xv6-k230/
├── Makefile              # 主构建文件
├── README.md             # 本文档
│
├── kernel/               # 内核源码
│   ├── entry.S           # 内核入口 (M-Mode启动)
│   ├── start.c           # M-Mode初始化 (切换到S-Mode)
│   ├── main.c            # S-Mode主函数 (子系统初始化)
│   ├── vm.c / vm.h       # 虚拟内存管理
│   ├── kalloc.c          # 物理内存分配
│   ├── proc.c / proc.h   # 进程管理
│   ├── trap.c            # 陷阱处理
│   ├── trampoline.S      # 用户/内核态切换
│   ├── syscall.c/.h      # 系统调用分发
│   ├── sysfile.c         # 文件相关系统调用
│   ├── sysproc.c         # 进程相关系统调用
│   ├── fs.c / fs.h       # 文件系统核心
│   ├── bio.c / buf.h     # 块设备缓冲层
│   ├── log.c             # 日志层 (事务支持)
│   ├── file.c / file.h   # 文件描述符层
│   ├── pipe.c            # 管道实现
│   ├── exec.c            # ELF加载器
│   ├── uart.c            # UART驱动 (DW8250)
│   ├── plic.c            # PLIC中断控制器
│   ├── timer.c           # RISC-V定时器
│   ├── k230_sd.c/.h      # SD卡驱动 (SDHCI 3.0)
│   ├── wdt.c             # 看门狗驱动
│   ├── spinlock.c/.h     # 自旋锁
│   ├── sleeplock.c/.h    # 睡眠锁
│   ├── string.c          # 内核字符串函数
│   └── memlayout.h       # 内存布局定义
│
├── user/                 # 用户程序
│   ├── init.c            # 初始化进程 (PID 1)
│   ├── sh.c              # Shell
│   ├── ls.c              # ls命令
│   ├── cat.c             # cat命令
│   ├── echo.c            # echo命令
│   ├── grep.c            # grep命令
│   ├── rm.c              # rm命令
│   ├── mkdir.c           # mkdir命令
│   ├── usertests.c       # 测试套件
│   ├── ulib.c            # 用户库
│   ├── umalloc.c         # 用户态malloc
│   ├── printf.c          # 用户态printf
│   ├── usys.pl           # 系统调用存根生成器
│   └── user.h            # 用户态头文件
│
└── mkfs/                 # 文件系统构建工具
    └── mkfs.c            # 创建fs.img镜像
```

---

## 🔧 开发指南

### 添加系统调用

1. **定义系统调用号** ([kernel/syscall.h](kernel/syscall.h))
   ```c
   #define SYS_mynewcall 22
   ```

2. **声明并注册内核函数** ([kernel/syscall.c](kernel/syscall.c))
   ```c
   extern uint64 sys_mynewcall(void);
   
   static uint64 (*syscalls[])(void) = {
       // ...
       [SYS_mynewcall] sys_mynewcall,
   };
   ```

3. **实现系统调用** ([kernel/sysproc.c](kernel/sysproc.c) 或 [kernel/sysfile.c](kernel/sysfile.c))
   ```c
   uint64 sys_mynewcall(void) {
       int arg1;
       if(argint(0, &arg1) < 0)
           return -1;
       // ... 实现逻辑
       return 0;
   }
   ```

4. **添加用户态接口** ([user/usys.pl](user/usys.pl))
   ```perl
   entry("mynewcall");
   ```

5. **声明用户函数** ([user/user.h](user/user.h))
   ```c
   int mynewcall(int arg1);
   ```

### 添加用户程序

1. **创建源文件** `user/myapp.c`
   ```c
   #include "kernel/types.h"
   #include "user/user.h"
   
   int main(int argc, char *argv[]) {
       printf("Hello from myapp\n");
       exit(0);
   }
   ```

2. **修改Makefile** 添加到 `UPROGS`
   ```makefile
   UPROGS=\
       # ...
       $U/_myapp
   ```

3. **重新编译**
   ```bash
   make
   ```

### 调试技巧

#### 串口日志
```c
// kernel/defs.h 提供 printf
printf("debug: value=%d\n", x);
```

#### 内核panic
```c
// 触发内核panic并打印栈帧
panic("something went wrong");
```

#### GDB调试 (JTAG)
```bash
# 需要硬件JTAG连接
riscv64-unknown-elf-gdb kernel/kernel
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

#### 查看寄存器状态
```c
// 在trap.c或其他位置打印trapframe
printf("epc=%p ra=%p sp=%p\n", p->trapframe->epc, 
       p->trapframe->ra, p->trapframe->sp);
```

---

## 🐛 已知限制与注意事项

### 硬件限制
- **SD卡性能**: 使用PIO模式 (无DMA), 吞吐量受限
- **无中断I/O**: SD卡操作采用轮询, 阻塞CPU
- **固定分区**: xv6文件系统位置硬编码为LBA 0xf000

### 系统限制
- **进程数量**: 最多64个进程 (NPROC=64)
- **打开文件**: 每进程16个 (NOFILE=16), 系统100个 (NFILE=100)
- **文件大小**: 最大 12+256 = 268块 = 268KB (MAXFILE限制)
- **文件系统**: 固定2000块 (FSSIZE=2000, 约2MB)

### K230特有注意事项
- **PLIC解锁**: 必须在M-Mode写入 `PLIC_CTRL` (0x0f01ffffc) = 1
- **T-Head扩展**: 需开启 `MXSTATUS.THEADISAEE` 位以支持MMU
- **定时器**: 使用 `stimecmp` CSR (非CLINT), 需MENVCFG.STCE=1
- **双核**: 代码支持2核, 但默认调度较简单 (轮转)

### 内存测试调整
- `sbrkfail`: 针对1GB内存调整分配限制 (原版xv6为128MB)
- `copyin`/`copyout`: 添加K230特定MMIO地址保护测试

---

## 📚 参考资料

### 官方文档
- [xv6 Book (RISC-V Edition)](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf) - MIT教学手册
- [RISC-V Privileged Specification](https://riscv.org/specifications/) - 架构规范
- [RISC-V Assembly Reference](https://github.com/riscv-non-isa/riscv-asm-manual/blob/master/riscv-asm.md)

### K230平台
- CanMV-K230 数据手册 (中文)
- [K230 SDK Documentation](https://github.com/kendryte/k230_sdk)
- C908 Core Manual (T-Head半导体)

### 相关项目
- [MIT xv6 官方仓库](https://github.com/mit-pdos/xv6-riscv)
- [xv6-riscv 课程网站](https://pdos.csail.mit.edu/6.828/2023/index.html)

---

## 🤝 贡献指南

欢迎提交Issue和Pull Request！

### 开发流程
1. Fork本仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启Pull Request

### 代码规范
- 遵循xv6代码风格 (K&R缩进, 2空格)
- 添加必要的注释 (特别是K230特定的硬件操作)
- 测试通过 `usertests` 验证

---

## 📄 许可证

本项目基于MIT许可证开源，继承自原版xv6项目。

```
Copyright (c) 2006-2023 Frans Kaashoek, Robert Morris, Russ Cox,
                        Massachusetts Institute of Technology

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files...
```

---

## 🙏 致谢

- **MIT PDOS Lab** - 原版xv6操作系统
- **Canaan Creative** - K230开发板支持
- **RISC-V Foundation** - 开放指令集架构
- **T-Head Semiconductor** - C908核心文档

---

<div align="center">

**⭐ 如果这个项目对你有帮助，请给个Star！ ⭐**

Made with ❤️ by xv6-k230 Contributors

</div>