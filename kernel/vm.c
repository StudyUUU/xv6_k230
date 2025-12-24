#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

// 全局变量：内核根页表
pagetable_t kernel_pagetable;

extern char etext[];      // 内核代码结束地址
extern char trampoline[], uservec[];  // 来自 trampoline.S
extern struct proc proc[NPROC];  // 进程表（定义在 proc.c）

/*
 * xv6-k230 虚拟内存管理
 * 
 * RISC-V Sv39 方案有三级页表：L2 -> L1 -> L0
 * 每一级页表都是一个 4KB 的页，里面包含 512 个 PTE (Page Table Entry)。
 * 
 * 虚拟地址 (64 位) 划分:
 *   39..63 -- 必须为零
 *   30..38 -- 9 位 L2 索引
 *   21..29 -- 9 位 L1 索引
 *   12..20 -- 9 位 L0 索引
 *    0..11 -- 12 位页内偏移
 */

// ============================================================================
// 基础页表操作函数
// ============================================================================

/*
 * walk - 在页表中查找虚拟地址对应的 PTE
 * @pagetable: 根页表地址
 * @va: 虚拟地址
 * @alloc: 如果为 1，则在查找失败时自动分配缺失的页表页
 * 
 * 返回: 指向 L0 级 PTE 的指针，失败返回 0
 */
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
      panic("walk");

  // 从 L2 遍历到 L1，最后返回 L0 的 PTE
  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)]; // 获取当前级的 PTE
    if(*pte & PTE_V) {
      // 如果该 PTE 有效，说明下一级页表存在
      pagetable = (pagetable_t)PTE2PA(*pte); // 继续走向下一级
    } else {
      // 如果无效，且 alloc 为 0，说明查找失败
      if(!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
        return 0;

      memset(pagetable, 0, PGSIZE); 
      
      // 将新页表的物理地址写入当前 PTE，并标记为有效 (V)
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  // 最后返回 L0 页表的 PTE 地址
  return &pagetable[PX(0, va)];
}

/*
 * mappages - 创建虚拟地址到物理地址的映射
 * @pagetable: 页表
 * @va: 起始虚拟地址
 * @size: 映射大小（字节）
 * @pa: 起始物理地址
 * @perm: 权限位（PTE_R/W/X/U 等）
 * 
 * 返回: 成功返回 0，失败返回 -1
 * 注意: va 和 size 可能不对齐页边界
 */
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, uint64 perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  
  for(;;){
    // 使用 walk 找到地址 a 对应的 PTE，如果不存在则分配
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    
    // 如果该 PTE 已经被映射过了（PTE_V 为 1），说明重复映射
    if(*pte & PTE_V)
      panic("remap");
    
    // 写入物理地址 (pa) 和 权限 (perm)，并标记 Valid
    *pte = PA2PTE(pa) | perm | PTE_V;

    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

/*
 * uvmunmap - 取消映射并可选释放物理页
 * @pagetable: 页表
 * @va: 起始虚拟地址（必须页对齐）
 * @npages: 页数
 * @do_free: 如果为 1，释放映射的物理页
 */
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0) // 叶子页表项已分配?
      continue;   
    if((*pte & PTE_V) == 0)  // 物理页已分配?
      continue;
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

/*
 * freewalk - 递归释放页表页
 * 所有叶子映射必须已经被移除
 */
void
freewalk(pagetable_t pagetable)
{
  // 页表中有 2^9 = 512 个 PTE
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // 此 PTE 指向下一级页表
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// ============================================================================
// 内核虚拟内存管理
// ============================================================================

/*
 * kvmmap - 辅助函数：简化 mappages 调用，失败时 panic
 */
void kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, uint64 perm) {
  if(mappages(pagetable, va, sz, pa, perm) != 0) {
    panic("kvmmap");
  }
}

/*
 * kvmmake - 创建内核页表
 * 
 * 映射内容：
 * 1. 设备 MMIO (UART0, WDT0, PLIC) - 使用 PTE_IO 标志
 * 2. 内核代码段 - RX 权限，使用 PTE_THEAD_MAEE
 * 3. 内核数据段 + 物理内存 - RW 权限，使用 PTE_THEAD_MAEE
 * 4. Trampoline 页 - 共享 U/S 模式
 * 5. 每个进程的内核栈
 */
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t)kalloc();
  if(kpgtbl == 0)
    panic("kvmmake: no memory");
  memset(kpgtbl, 0, PGSIZE);

  // -----------------------------
  // 1. 设备 (MMIO) - 使用 PTE_IO 禁用缓存，强制顺序访问
  // -----------------------------
  kvmmap(kpgtbl,
         UART0,
         UART0,
         PGSIZE,
         PTE_R | PTE_W | PTE_A | PTE_D | PTE_IO);

  kvmmap(kpgtbl,
         WDT0_BASE,
         WDT0_BASE,
         PGSIZE,
         PTE_R | PTE_W | PTE_A | PTE_D | PTE_IO);

  kvmmap(kpgtbl,
         PLIC,
         PLIC_PA,
         0x4000000,
         PTE_R | PTE_W | PTE_A | PTE_D | PTE_IO);

  // -----------------------------
  // 2. 内核代码段 (RX) - 使用 PTE_THEAD_MAEE 启用缓存和原子操作
  // -----------------------------
  kvmmap(kpgtbl,
         KERNBASE,
         KERNBASE,
         (uint64)etext - KERNBASE,
         PTE_R | PTE_X | PTE_A | PTE_THEAD_MAEE);

  // -----------------------------
  // 3. 内核数据段 + 物理 RAM (RW)
  // -----------------------------
  kvmmap(kpgtbl,
         (uint64)etext,
         (uint64)etext,
         PHYSTOP - (uint64)etext,
         PTE_R | PTE_W | PTE_A | PTE_D | PTE_THEAD_MAEE);

  // -----------------------------
  // 4. Trampoline (用户/内核共享) - 去掉 PTE_U 和 MAEE
  // -----------------------------
  kvmmap(kpgtbl,
        TRAMPOLINE,
        (uint64)trampoline,
        PGSIZE,
        PTE_R | PTE_X | PTE_A);

  // -----------------------------
  // 5. 每个进程的内核栈
  // -----------------------------
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}

/*
 * kvminit - 初始化内核页表全局变量
 */
void kvminit(void)
{
  kernel_pagetable = kvmmake();
}

/*
 * kvminithart - 在当前 hart 上激活内核页表
 * 写入 satp 寄存器，刷新 TLB
 */
void kvminithart() {
  // 等待之前对页表内存的写入完成
  sfence_vma();

  // 写入 satp 寄存器
  // MAKE_SATP 宏在 riscv.h 中定义，设置模式为 Sv39 并填入根页表物理页号
  w_satp(MAKE_SATP(kernel_pagetable));

  // 刷新 TLB (快表)
  // 必须执行！否则 CPU 可能还缓存着旧的地址转换规则
  sfence_vma();
}

// ============================================================================
// 用户虚拟内存管理
// ============================================================================

/*
 * uvmcreate - 创建一个空的用户页表
 * 返回: 页表地址，失败返回 0
 */
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

/*
 * proc_pagetable - 为进程创建用户页表
 * @p: 进程结构体指针
 * 
 * 映射 trampoline 和 trapframe 到用户地址空间高端
 */
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // trampoline: U/S 共享执行
  if(mappages(pagetable,
              TRAMPOLINE,
              PGSIZE,
              (uint64)trampoline,
              PTE_R | PTE_X | PTE_A) < 0)
    goto bad;

  // trapframe: 只给 S-mode 用，但在用户页表里
  if(mappages(pagetable,
              TRAPFRAME,
              PGSIZE,
              (uint64)p->trapframe,
              PTE_R | PTE_W | PTE_A | PTE_D | PTE_THEAD_MAEE) < 0)
    goto bad;

  return pagetable;

bad:
  panic("proc_pagetable: mappages failed");
  uvmfree(pagetable, 0);
  return 0;
}

/*
 * proc_freepagetable - 释放进程的页表
 * @pagetable: 用户页表
 * @sz: 进程大小
 * 
 * 取消映射 trampoline 和 trapframe，然后释放页表
 */
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

/*
 * proc_mapstacks - 为每个进程分配并映射内核栈
 * @kpgtbl: 内核页表
 * 
 * 每个进程的内核栈映射到高地址，下方有保护页（guard page）用于检测栈溢出
 * 使用 KSTACK(pid) 宏计算虚拟地址
 */
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));

    p->kstack = va;

    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W | PTE_THEAD_MAEE | PTE_A | PTE_D);
  }
}

/*
 * uvminit - 将初始用户代码加载到地址 0
 * @pagetable: 用户页表
 * @src: 源代码地址
 * @sz: 大小（必须小于一页）
 * 
 * 用于创建第一个用户进程
 */
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("uvminit: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U|PTE_A|PTE_D|PTE_THEAD_MAEE);

  memmove(mem, src, sz);
}

/*
 * uvmalloc - 扩展进程内存，从 oldsz 到 newsz
 * @pagetable: 用户页表
 * @oldsz: 旧大小
 * @newsz: 新大小
 * @xperm: 额外权限位（PTE_W 或 PTE_X）
 * 
 * 返回: 新大小，失败返回 0
 * 注意: oldsz 和 newsz 不需要页对齐
 */
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm|PTE_A|PTE_D|PTE_THEAD_MAEE) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

/*
 * uvmdealloc - 缩小进程内存，从 oldsz 到 newsz
 * @pagetable: 用户页表
 * @oldsz: 旧大小
 * @newsz: 新大小
 * 
 * 返回: 新大小
 * 注意: oldsz 和 newsz 不需要页对齐，newsz 也不必小于 oldsz
 */
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

/*
 * uvmfree - 释放用户内存页和页表页
 * @pagetable: 用户页表
 * @sz: 进程大小
 */
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

/*
 * uvmcopy - 复制父进程的内存到子进程
 * @old: 父进程页表
 * @new: 子进程页表
 * @sz: 复制大小
 * 
 * 同时复制页表和物理内存
 * 返回: 成功返回 0，失败返回 -1 并释放已分配的页
 */
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

/*
 * copyout - 从内核复制到用户空间
 * @pagetable: 用户页表
 * @dstva: 目标虚拟地址
 * @src: 源地址（内核）
 * @len: 长度
 * 
 * 返回: 成功返回 0，失败返回 -1
 */
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
    pte = walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 ||
       (*pte & PTE_W) == 0)
      return -1;
    pa0 = PTE2PA(*pte);
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

/*
 * copyin - 从用户空间复制到内核
 * @pagetable: 用户页表
 * @dst: 目标地址（内核）
 * @srcva: 源虚拟地址
 * @len: 长度
 * 
 * 返回: 成功返回 0，失败返回 -1
 */
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    if(va0 >= MAXVA)
      return -1;
    pte = walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
      return -1;
    pa0 = PTE2PA(*pte);
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

/*
 * copyinstr - 从用户空间复制以 null 结尾的字符串到内核
 * @pagetable: 用户页表
 * @dst: 目标地址（内核）
 * @srcva: 源虚拟地址
 * @max: 最大长度
 * 
 * 复制直到遇到 '\0' 或达到 max
 * 返回: 成功返回 0，失败返回 -1
 */
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;
  pte_t *pte;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    if(va0 >= MAXVA)
      return -1;
    pte = walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
      return -1;
    pa0 = PTE2PA(*pte);
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}