// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PA2INDEX(pa) (((uint64)pa)/PGSIZE) //将物理地址转化为计数器下标
uint64  n_cow[PHYSTOP/PGSIZE];//计数器,一个物理页可能同时有多个进程的虚拟页指向他,所以freepage时当物理页计数器降为0时才真正释放他

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    n_cow[PA2INDEX(p)] = 1; // 初始化的时候把每个物理页的计数器设为1,及计为free
    kfree(p);
  }
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

  acquire(&kmem.lock);
  int remain = --n_cow[PA2INDEX(pa)];
  release(&kmem.lock);
  
  if (remain > 0) //计数器降为0后才真正释放,否则不释放.
    return;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    int idx = PA2INDEX(r);
    if (n_cow[idx] != 0)
      panic("kalloc: n_cow[idx] != 0");
    n_cow[idx] = 1; // 新分配的物理页的计数器初始化为1
  }
  return (void*)r;
}

//辅助调整计数器的函数,这样就不用在vm.c中声明n_cow等操作而可以直接调用
void
addcow(uint64 pa, int num) {
  if (pa >= PHYSTOP)//超过物理内存最大地址
    panic("addcow: pa too big");
  //上锁避免冲突
  acquire(&kmem.lock);
  n_cow[PA2INDEX(pa)] += num;
  release(&kmem.lock);
}
