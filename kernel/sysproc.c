#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "vm.h"

uint64
sys_fork(void)
{
  // printf("kfork called\n");
  return kfork();
}

uint64
sys_exit(void)
{
  // printf("kexit called\n");
  int n;
  // 获取退出状态码 exit(status)
  argint(0, &n);
  kexit(n);
  return 0;  // 不会执行到这里，因为 kexit 不返回
}

uint64
sys_getpid(void)
{
  // printf("getpid called\n");
  return myproc()->pid;
}

uint64
sys_wait(void)
{
  // printf("kwait called\n");
  uint64 p;
  // 获取用户态传入的地址指针 wait(&status)
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}
