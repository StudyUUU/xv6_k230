#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"

extern char end[]; // kernel.ld 中定义的内核结束地址

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


// 初始化物理内存
void kinit() {
  initlock(&kmem.lock, "kmem");

  // 从内核结束的地方开始，一直到物理内存结束
  // 把每一页都释放掉(kfree)，这样它们就进入了 freelist
  char *p = (char*)PGROUNDUP((uint64)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)
    kfree(p);
}

// 释放一页物理内存
void kfree(void *pa) {
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
  {
    panic("kfree error");
  }

  memset(pa, 1, PGSIZE); // 释放时用垃圾数据(1)填充，方便调试发现野指针

  acquire(&kmem.lock);

  r = (struct run*)pa;
  r->next = kmem.freelist;
  kmem.freelist = r;

  release(&kmem.lock);
}

// 分配一页物理内存
void *kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 0, PGSIZE); 
  return (void*)r;
}