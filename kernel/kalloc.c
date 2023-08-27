// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; ++i)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 禁用中断
  push_off();
  // 获取当前 CPU 核心的 ID
  int id = cpuid();
  // 重新启用中断
  pop_off();

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();	    // 关闭中断
  int id = cpuid(); // 获取当前CPU的ID
  pop_off();	    // 恢复中断

  //下面这部分代码其实就是把原本的kmem变成kmem[id]
  acquire(&kmem[id].lock); // 获取当前cpu的内存分配器锁
  r = kmem[id].freelist;   // 获取当前cpu的空闲列表的第一个物理页
  if(r) {  //如果有空闲页,则更新空闲列表
    kmem[id].freelist = r->next;
  }
  else {  //如果没有,则需要去"窃取"其他cpu的空闲列表
    for (int i = 1; i < NCPU; ++i) {
      // 从右边的第一个邻居cpu开始窃取,偷到自己的前一个邻居cpu为止
      int victim = (id + i) % NCPU;
    
      // 下面这部分代码同上,只不过换了个主角
      acquire(&kmem[victim].lock);// 获取其他CPU的内存分配器锁
      r = kmem[victim].freelist;
      if(r){ //如果找到,就更新空闲列表.释放锁并break出去
        kmem[victim].freelist = r->next;
        release(&kmem[victim].lock);
        break;
      }
      else {
        release(&kmem[victim].lock); //无论是否成功,当前操作结束后都要释放锁
      }
    }
  }
  release(&kmem[id].lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
