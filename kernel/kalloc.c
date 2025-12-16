#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"

extern char end[]; // kernel.ld 中定义的内核结束地址

struct run {
  struct run *next;
};

struct {
  struct run *freelist;
} kmem;

void kfree(void *pa) {
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
  {
    panic("kfree error");
  }

  // 把这一页内存填满垃圾数据(1)，方便调试发现野指针
  // memset(pa, 1, PGSIZE); (暂时还没有 memset，先略过)

  r = (struct run*)pa;
  r->next = kmem.freelist;
  kmem.freelist = r;
}

// 初始化物理内存
void kinit() {
  // 从内核结束的地方开始，一直到物理内存结束
  // 把每一页都释放掉(kfree)，这样它们就进入了 freelist
  char *p = (char*)PGROUNDUP((uint64)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)
    kfree(p);
}

// 分配一页物理内存
void *kalloc(void) {
  struct run *r;
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  return (void*)r;
}