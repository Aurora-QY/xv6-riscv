#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

#include "spinlock.h"
#include "proc.h"

//#include "vmcopyin.c"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// 递归地打印页表的内容
void vmprint_recursive(pagetable_t pagetable, int level) {
  // 遍历页表中的所有条目
  for (int i = 0; i < PTE_IN_PGTBL; ++i) {
  //for (int i = 0; i < 1024; ++i) {
    pte_t pte = pagetable[i];
    // 检查当前 PTE 是否有效
    if (pte & PTE_V) {
      uint64 pa = PTE2PA(pte);// 获取物理地址
      for (int j = 0; j < level; ++j) {// 打印缩进
        if (j)
          printf(" ");
        printf("..");
      }
      // 打印 PTE 编号、PTE 值和对应的物理地址
      printf("%d: pte %p pa %p\n", i, pte, pa);
      // 如果 PTE 没有读、写、执行权限，表示它指向另一个页表页
      if ((pte & (PTE_R | PTE_W | PTE_X)) == 0)
        vmprint_recursive((pagetable_t)pa, level+1);// 递归打印下一级的页表
    }
  }
}

void vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  vmprint_recursive(pagetable, 1);
} 


// 该函数用于将虚拟地址范围映射到物理地址，并设置对应的权限。
void ukvmmap(pagetable_t kpagetable, uint64 va, uint64 pa, uint64 sz, int perm) {
  // 调用mappages函数来将虚拟地址范围映射到物理地址范围，并设置权限。
  // 如果映射失败，触发panic，即终止程序执行。
  if(mappages(kpagetable, va, sz, pa, perm) != 0)
    panic("ukvmmap");
}

// 该函数用于初始化一个页表，并进行一些初始化映射。
pagetable_t ukvminit() {
  // 为内核页表分配一页内存（PGSIZE字节大小），并将其清零。
  pagetable_t kpagetable = (pagetable_t) kalloc();
  memset(kpagetable, 0, PGSIZE);

  // 将一些关键的硬件设备、内核代码和数据区域进行映射。

  // 映射UART0设备的物理地址到相应的虚拟地址，并设置读写权限。
  ukvmmap(kpagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // 映射VIRTIO0设备的物理地址到相应的虚拟地址，并设置读写权限。
  ukvmmap(kpagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // 映射CLINT设备的物理地址到相应的虚拟地址，并设置读写权限。
  ukvmmap(kpagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // 映射PLIC设备的物理地址到相应的虚拟地址，并设置读写权限。
  ukvmmap(kpagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // 映射内核代码和只读数据区域的物理地址到相应的虚拟地址，并设置读执行权限。
  ukvmmap(kpagetable, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // 映射从只读数据区域末尾到物理内存的结束地址，用于内核数据，设置读写权限。
  ukvmmap(kpagetable, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // 映射trampoline代码的物理地址到相应的虚拟地址，并设置读执行权限。
  ukvmmap(kpagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // 返回初始化后的内核页表。
  return kpagetable;
}

// 该函数用于在Hart（硬件线程）启动时初始化页表，以便让该Hart能够使用正确的虚拟地址映射。
void ukvminithart(pagetable_t kpagetable)
{
  // 调用w_satp函数，将传入的内核页表kpagetable的信息设置到SATP寄存器中。
  // SATP寄存器是RISC-V体系结构中的一个寄存器，用于指定页表的位置和格式。
  w_satp(MAKE_SATP(kpagetable));

  // 调用sfence_vma函数，执行虚拟内存地址的同步操作。
  // sfence_vma指令用于确保之前的虚拟内存操作对所有Hart都是可见的，以避免不一致性。
  sfence_vma();
}

// 和mappages一模一样, 只不过不再panic remapping, 直接强制复写
int umappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) {
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 函数目的：将给定的旧页表中指定范围的页复制到新页表中。
int pagecopy(pagetable_t oldpage, pagetable_t newpage, uint64 begin, uint64 end) {
  pte_t *pte;         // 页表项指针
  uint64 pa, i;       // 物理地址，循环变量
  uint flags;         // 标志位
  begin = PGROUNDUP(begin); // 向上对齐begin到页边界

  // 遍历从begin到end范围内的虚拟地址，每次递增一个页的大小
  for (i = begin; i < end; i += PGSIZE) {
    // 使用walk函数，在旧页表中查找虚拟地址i对应的页表项
    if ((pte = walk(oldpage, i, 0)) == 0)
      panic("pagecopy walk oldpage nullptr"); // 如果找不到，触发panic，终止程序执行
    // 检查页表项是否有效（Valid标志位被设置）
    if ((*pte & PTE_V) == 0)
      panic("pagecopy oldpage pte not valid"); // 如果无效，触发panic，终止程序执行
    pa = PTE2PA(*pte); // 从页表项获取物理地址
    flags = PTE_FLAGS(*pte) & (~PTE_U); // 获取标志位并将用户标志（U flag）抹去

    // 使用umappages函数，在新页表中映射虚拟地址i到物理地址pa，使用标志位flags
    if (umappages(newpage, i, PGSIZE, pa, flags) != 0) {
      goto err; // 如果映射失败，跳转到err标签处理
    }
  }
  return 0; // 成功时返回0

err:
  // 在发生错误的情况下，取消刚刚建立的映射，并返回-1
  uvmunmap(newpage, 0, i / PGSIZE, 1);
  return -1;
}

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(myproc()->kpagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  return copyin_new(pagetable, dst, srcva, len);
  
//  uint64 n, va0, pa0;
//
//  while(len > 0){
//    va0 = PGROUNDDOWN(srcva);
//    pa0 = walkaddr(pagetable, va0);
//    if(pa0 == 0)
//      return -1;
//    n = PGSIZE - (srcva - va0);
//    if(n > len)
//      n = len;
//    memmove(dst, (void *)(pa0 + (srcva - va0)), n);
//
//    len -= n;
//    dst += n;
//    srcva = va0 + PGSIZE;
//  }
//  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  return copyinstr_new(pagetable, dst, srcva, max);
  
  
//  uint64 n, va0, pa0;
//  int got_null = 0;
//
//  while(got_null == 0 && max > 0){
//    va0 = PGROUNDDOWN(srcva);
//    pa0 = walkaddr(pagetable, va0);
//    if(pa0 == 0)
//      return -1;
//    n = PGSIZE - (srcva - va0);
//    if(n > max)
//      n = max;
//
//    char *p = (char *) (pa0 + (srcva - va0));
//    while(n > 0){
//      if(*p == '\0'){
//        *dst = '\0';
//        got_null = 1;
//        break;
//      } else {
//        *dst = *p;
//      }
//      --n;
//      --max;
//      p++;
//      dst++;
//    }
//
//    srcva = va0 + PGSIZE;
//  }
//  if(got_null){
//    return 0;
//  } else {
//    return -1;
//  }
}
