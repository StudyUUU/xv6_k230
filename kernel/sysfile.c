#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"

// 这是一个简化的 write，直接忽略 fd，强制输出到 console
// 真正的 OS 需要检查 p->ofile[fd]
uint64
sys_write(void)
{
  int fd;
  uint64 p; // 用户缓冲区的虚拟地址
  int n;    // 写入长度

  // 1. 使用你封装好的函数获取参数
  // write(fd, buf, len) -> a0, a1, a2
  argint(0, &fd);    // 获取 a0
  argaddr(1, &p);    // 获取 a1
  argint(2, &n);     // 获取 a2

//   printf("[Process %d] write(fd=%d, addr=%p, len=%d)\n", myproc()->pid, fd, p, n);
  
  // 2. 简单的参数检查
  if(n < 0 || n > 1024) { // 限制单次打印长度，防止内核栈溢出
      return -1;
  }
  
  // 3. 在内核栈上分配缓冲区
  // 为什么要 +1？为了最后手动补一个 '\0' 安全打印
  char kbuf[128]; 
  
  // 如果用户请求写的太长，我们分块处理或截断
  // 这里为了演示 K230 "Hello"，我们直接截断到内核 buffer 大小
  if (n > sizeof(kbuf) - 1) 
      n = sizeof(kbuf) - 1;
	// 4. 核心：从用户态搬运数据到内核态
	struct proc *pr = myproc();

  if(copyin(pr->pagetable, kbuf, p, n) < 0) {
      printf("[sys_write] copyin failed: bad address %p\n", p);
      return -1;
  }

  // 5. 安全封口
  kbuf[n] = 0;

  // 6. 执行输出
  if(fd == 1 || fd == 2) { // stdout or stderr
      printf("%s", kbuf); 
  } else {
      // 暂时还不支持文件写入，只打印调试信息
      
  }

  return n; // 返回实际写入的字节数
}