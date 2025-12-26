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

  char *p = (char*)PGROUNDUP((uint64)end); // 从内核代码/数据结束的位置开始，end 刚好为内核结束地址，也就是堆顶
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)
    kfree(p); // 把剩下的所有页，一页一页地“释放”进空闲链表
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

  r = (struct run*)pa; // 头插法加入空闲链表
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

  // 因为 r->next 占用了前 8 字节，分配出去前必须清零，
  // 否则调用者会读到这个残留的指针数据，造成安全隐患。
  // 这里清零整个页，简单粗暴。
  if(r)
    memset((char*)r, 0, PGSIZE);  

  return (void*)r;
}