#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"

extern char end[];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// 物理页引用计数，用于 COW
#define KREF_INDEX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)
#define KREF_COUNT     ((PHYSTOP - KERNBASE) / PGSIZE)
int krefcount[KREF_COUNT];

void krefpage(void *pa) {
  uint64 idx = KREF_INDEX(pa);
  if(idx >= KREF_COUNT) return;
  acquire(&kmem.lock);
  krefcount[idx]++;
  release(&kmem.lock);
}

int kgetref(void *pa) {
  uint64 idx = KREF_INDEX(pa);
  if(idx >= KREF_COUNT) return 1;
  acquire(&kmem.lock);
  int ref = krefcount[idx];
  release(&kmem.lock);
  return ref;
}

// 初始化物理内存
void kinit() {
  initlock(&kmem.lock, "kmem");
  char *p = (char*)PGROUNDUP((uint64)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE) {
    krefcount[KREF_INDEX(p)] = 1;
    kfree(p);
  }
}

// 释放一页物理内存
void kfree(void *pa) {
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 idx = KREF_INDEX(pa);
  if(idx < KREF_COUNT) {
    acquire(&kmem.lock);
    if(krefcount[idx] > 1) {
      krefcount[idx]--;
      release(&kmem.lock);
      return;
    }
    krefcount[idx] = 0;
    release(&kmem.lock);
  }

  memset(pa, 1, PGSIZE);

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

  if(r) {
    memset((char*)r, 0, PGSIZE);
    krefcount[KREF_INDEX(r)] = 1;
  }
  return (void*)r;
}
