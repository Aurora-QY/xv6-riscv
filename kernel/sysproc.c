#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"


//从用户空间获取定时器间隔和用户提供的函数指针，然后将这些信息存储在当前进程的相关字段中。当定时器触发时，内核就知道要调用哪个函数以及何时调用。
uint64
sys_sigalarm(void)
{
  int interval;            // 定时器间隔
  uint64 handler;          // 用户提供的函数指针
  struct proc *p;          // 指向当前进程的指针

  // 从用户空间获取定时器间隔和函数指针
  if (argint(0, &interval) < 0 || argaddr(1, &handler) < 0 || interval < 0) {
    return -1;             // 参数错误，返回 -1 表示失败
  }

  p = myproc();            // 获取当前进程的指针
  p->interval = interval;  // 设置定时器间隔
  p->handler = handler;    // 存储用户提供的函数指针
  p->ticks = 0;            // 重置 ticks 计数器，用于计数定时器的触发次数

  return 0;                // 返回 0 表示成功
}

uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  *p->trapframe = *p->pretrapframe; //将trapframe还原成中断前的状态
  p->ticks = 0;
  return 0;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  backtrace(); // 调用回溯
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
