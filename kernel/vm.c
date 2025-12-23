#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

// 全局变量：内核根页表
pagetable_t kernel_pagetable;

extern char etext[];      // 内核代码结束地址
extern char trampoline[], uservec[];  // 来自 trampoline.S

/*
 * 这里的逻辑是 XV6 虚拟内存的核心。
 * RISC-V Sv39 方案有三级页表：L2 -> L1 -> L0
 * 每一级页表都是一个 4KB 的页，里面包含 512 个 PTE (Page Table Entry)。
 */

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// riscv Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
      panic("walk");

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

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
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
    
    // 如果该 PTE 已经被映射过了（PTE_V 为 1），说明我们要么在重复映射，要么出错了
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

// 辅助函数：简化 mappages 调用，如果失败直接死循环
void kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, uint64 perm) {
  if(mappages(pagetable, va, sz, pa, perm) != 0) {
    panic("kvmmap");
  }
}

// Create the kernel page table.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t)kalloc();
  if(kpgtbl == 0)
    panic("kvmmake: no memory");
  memset(kpgtbl, 0, PGSIZE);

  // -----------------------------
  // 1. Devices (MMIO)
  // -----------------------------
  kvmmap(kpgtbl,
         UART0,
         UART0,
         PGSIZE,
         PTE_R | PTE_W | PTE_A | PTE_D | PTE_IO);

  kvmmap(kpgtbl,
         PLIC,
         PLIC_PA,
         0x4000000,
         PTE_R | PTE_W | PTE_A | PTE_D | PTE_IO);

  // -----------------------------
  // 2. Kernel text (RX)
  // -----------------------------
  kvmmap(kpgtbl,
         KERNBASE,
         KERNBASE,
         (uint64)etext - KERNBASE,
         PTE_R | PTE_X | PTE_A | PTE_THEAD_MAEE);

  // -----------------------------
  // 3. Kernel data + physical RAM (RW)
  // -----------------------------
  kvmmap(kpgtbl,
         (uint64)etext,
         (uint64)etext,
         PHYSTOP - (uint64)etext,
         PTE_R | PTE_W | PTE_A | PTE_D | PTE_THEAD_MAEE);

  // -----------------------------
  // 4. Trampoline (shared U/S)
  // -----------------------------
  kvmmap(kpgtbl,
        TRAMPOLINE,
        (uint64)trampoline,
        PGSIZE,
        PTE_R | PTE_X | PTE_A); // 只有 R, X, A。去掉 PTE_U 和 MAEE

  // -----------------------------
  // 5. Kernel stacks (one per proc)
  // -----------------------------
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}


void kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// 开启分页机制 (激活地图)
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

void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0) // leaf page table entry allocated?
      continue;   
    if((*pte & PTE_V) == 0)  // has physical page been allocated?
      continue;
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}
// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}
// create an empty user page table.
// returns 0 if out of memory.
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


// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
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

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
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

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
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

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
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

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
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

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
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

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
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