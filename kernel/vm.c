#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"



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
      
      // 如果 alloc 为 1，我们刚分配了一个新页作为下一级页表
      // 这里的 memset 已经在 kalloc 里做过了，为了保险可以不写，但为了严谨最好确认一下
      // memset(pagetable, 0, PGSIZE); 
      
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
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
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

void test_vm() {
    printf("\n=== [TEST] Virtual Memory Toolchain ===\n");

    // 1. 分配一个根页表
    pagetable_t root = (pagetable_t)kalloc();
    if(root == 0) {
        printf("Error: kalloc failed to allocate root page table\n");
        return;
    }
    memset(root, 0, PGSIZE);
    printf("1. Root page table created at: %p\n", root);

    // 2. 模拟映射：将内核虚拟地址映射到物理内存
    // 虚拟地址: 0x80000000, 物理地址: 0x00400000 (假设的一块内存)
    uint64 va = 0x80000000L;
    uint64 pa = 0x00400000L;
    int perm = PTE_R | PTE_W | PTE_X;

    printf("2. Mapping VA %p to PA %p...\n", va, pa);
    if(mappages(root, va, PGSIZE, pa, perm) != 0) {
        printf("   FAIL: mappages error\n");
        return;
    }
    printf("   Success: mappages linked the addresses.\n");

    // 3. 验证 walk 函数
    printf("3. Verifying with walk(va=%p)...\n", va);
    pte_t *pte = walk(root, va, 0);

    if(pte == 0) {
        printf("   FAIL: walk could not find the path to VA %p\n", va);
    } else {
        uint64 content = *pte;
        uint64 found_pa = PTE2PA(content);
        
        printf("   - Found PTE at: %p\n", pte);
        printf("   - PTE Content:  %p\n", content);
        printf("   - Extracted PA: %p\n", found_pa);
        printf("   - Flags: [ %s %s %s %s ]\n", 
               (content & PTE_V) ? "V" : "-",
               (content & PTE_R) ? "R" : "-",
               (content & PTE_W) ? "W" : "-",
               (content & PTE_X) ? "X" : "-");

        // 最终校验
        if(found_pa == pa && (content & PTE_V)) {
            printf("=== [PASS] walk and mappages are working correctly! ===\n\n");
        } else {
            printf("=== [FAIL] Address mismatch or invalid PTE! ===\n\n");
        }
    }
}