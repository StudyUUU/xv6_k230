#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"

uint64
sys_fork(void)
{
  printf("kfork called\n");

  return kfork();
}

uint64
sys_exit(void)
{
  printf("kexit called\n");

  int n;
  // 获取退出状态码 exit(status)
  argint(0, &n);
  kexit(n);
  return 0;  // 不会执行到这里，因为 kexit 不返回
}

uint64
sys_getpid(void)
{
  printf("getpid called\n");

  return myproc()->pid;
}

uint64
sys_wait(void)
{
  printf("kwait called\n");

  uint64 p;
  // 获取用户态传入的地址指针 wait(&status)
  argaddr(0, &p);
  return kwait(p);
}