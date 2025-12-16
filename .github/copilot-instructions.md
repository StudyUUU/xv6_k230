# AI Agent Instructions for xv6 K230 Boot Project

## Project Overview

This is a minimal xv6 kernel bootstrap implementation for the Kendryte K230 RISC-V SoC. The project bridges from M-mode (machine mode) firmware to S-mode (supervisor mode) kernel execution with basic serial I/O.

**Key Goal**: Load at 0x00200000, initialize hardware, configure CPU privileges, and jump to `main()` in S-mode.

## Architecture & Boot Flow

### Three-Stage Boot Pipeline

1. **entry.S** (M-mode entry point)
   - Sets up per-CPU stacks with `stack0[4096]`
   - Stack calculation: `sp = stack0 + (hartid * 4096)`
   - Jumps to `start()` in start.c

2. **start.c** (M-mode initialization)
   - Configures privilege delegation: exceptions and interrupts → S-mode
   - Sets M-mode PMP (Physical Memory Protection) to allow S-mode full memory access
   - **Critical**: Without PMP configuration (`w_pmpaddr0()` + `w_pmpcfg0()`), S-mode instruction fetch fails
   - Uses `mret` to transition to S-mode at `main()`

3. **main.c** (S-mode kernel)
   - Currently minimal (prints welcome message, then idles)
   - Future: implement kernel services

### Hardware-Specific Details

**K230 UART**: 
- Base: `0x91400000`
- Uses DesignWare 8250 register layout (4-byte stride)
- Send: Wait for LSR bit 5 (THRE) before writing to THR
- Implemented in [uart.c](uart.c) as blocking writes

**Linker Layout** ([kernel.ld](kernel.ld)):
- Load address: 0x00200000 (K230-specific)
- Must preserve `.rodata` section for string constants
- Includes placeholder for trampoline code (trampsec)

## Critical Development Patterns

### CSR (Control Status Register) Operations

All privileged operations use inline assembly functions defined in [start.c](start.c):
```c
static inline uint64_t r_mstatus() { /* ... csrr mstatus ... */ }
static inline void w_medeleg(uint64_t x) { /* ... csrw medeleg ... */ }
```

**Pattern**: Read-modify-write for flags:
```c
unsigned long x = r_mstatus();
x &= ~MSTATUS_MPP_MASK;        // Clear bits
x |= MSTATUS_MPP_S;             // Set new value
w_mstatus(x);
```

### Known Pitfalls & Workarounds

1. **Interrupts**: Currently disabled - no S-mode interrupt vector (`stvec`) setup yet. Enabling without it causes crashes.

2. **Clock Interrupts**: K230's clock controller isn't the standard CLINT. Calling generic `timerinit()` causes hangs. Use K230-specific timer interface if needed.

3. **PMP Configuration**: Essential before `mret`. Without `w_pmpaddr0(0x3fff...ull)` and `w_pmpcfg0(0xf)`, S-mode cannot fetch instructions.

4. **String Constants**: Linker MUST include `.rodata` section, or `uart_puts()` calls with string literals fail.

## Build & Verification

**Build Command** (see [Makefile](Makefile)):
```bash
make clean
make all
```

**Output**: 
- `kernel.elf` - Linked ELF with symbols (for debugging)
- `kernel.bin` - Raw binary for flashing

**Compiler Flags**:
- `-ffreestanding -nostdlib`: No standard library (baremetal)
- `-mcmodel=medany -mno-relax`: Relaxation disabled for K230 compatibility
- `-ggdb`: Debug symbols

**Typical Issues**:
- Missing `riscv64-unknown-elf-*` toolchain → install RISC-V cross-compiler
- Linker error about undefined symbols → check function declarations match definitions
- "undefined reference to `_entry`" → ensure entry.S is in OBJS in Makefile

## Code Organization Principles

### File Responsibilities

- **entry.S**: Assembly-only, stack setup, immediate jump to start()
- **start.c**: Privilege/CPU setup, registers (CSRs), PMP, mret transition
- **uart.c**: Hardware I/O only (register writes), blocking design
- **main.c**: C-level kernel logic (currently stub)
- **kernel.ld**: Memory layout, load address, section ordering

### Naming Conventions

- **CSR accessors**: `r_<name>()` (read), `w_<name>()` (write)
- **UART**: `uart_putc()` (char), `uart_puts()` (string)
- **Bit masks**: `<REGISTER>_<FIELD>` e.g., `MSTATUS_MPP_S`

### Adding New Code

- **New C module**: Add `<name>.c`, add `<name>.o` to OBJS in Makefile
- **New assembly**: Add `<name>.S`, same Makefile pattern
- **Hardware I/O**: Isolate in separate `.c` file (like uart.c), minimize asm
- **Strings in S-mode**: Must use `uart_puts()` from uart.c - rely on linker's `.rodata`

## Integration Points & Dependencies

### External Interfaces

- **Bootloader**: Loads kernel.bin at 0x00200000, jumps to _entry in M-mode
- **K230 Serial Console**: Receives UART0 output (no input driver yet)
- **K230 Hardware**: Assumes hart0, mhartid CSR support

### Cross-Module Communication

- `entry.S` → `start()` (C function call)
- `start()` → `main()` (via mret + mepc register)
- `main.c` uses `uart_puts()` from uart.c for output
- All CSR operations in start.c; don't duplicate in main.c

## Testing & Debugging

**Manual Verification**:
1. Build: `make`
2. Flash kernel.bin to K230 at 0x00200000
3. Expect serial output: "xv6 on K230: Hello from S-mode!"

**Troubleshooting**:
- No output → Check UART base address (0x91400000) and baud rate compatibility
- Crashes on mret → Likely missing PMP setup in start.c
- String corruption → Check kernel.ld includes `.rodata`

**Future**: Add gdb stub or on-device debugger hooks as project grows.

## When Adding Features

1. **Interrupts**: Set up S-mode `stvec` before enabling `SIE` bits
2. **Memory Paging**: Implement virtual address translation, update `satp`
3. **New Peripherals**: Follow uart.c pattern (base address + register macros + inline I/O)
4. **Multi-core**: Expand `stack0` array, adjust PMP/hart iteration in start.c
