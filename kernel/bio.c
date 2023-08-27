// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// 使用取余操作计算哈希桶索引
#define bufhash(blockno) ((blockno) % NBUCKET)

struct {
  struct spinlock biglock;
  struct spinlock lock[NBUCKET]; // 改成13个锁
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];  // 改成13个桶
} bcache;

void
binit(void)
{
  struct buf *b;
  
  // 初始化大锁和桶锁
  initlock(&bcache.biglock, "bcache_biglock");
  for (int i = 0; i < NBUCKET; ++i)
    initlock(&bcache.lock[i], "bcache");

  // 创建缓冲区的链接列表
  for (int i = 0; i < NBUCKET; ++i) {
    bcache.head[i].next = &bcache.head[i];
    bcache.head[i].prev = &bcache.head[i];
  }
  // 初始化每个缓冲区
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *b2 = 0;

  int i = bufhash(blockno), min_ticks = 0;
  acquire(&bcache.lock[i]);

  // 检查块是否已缓存
  for(b = bcache.head[i].next; b != &bcache.head[i]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[i]);
      acquiresleep(&b->lock);
      return b;  // 返回已缓存的块，并增加引用计数
    }
  }
  release(&bcache.lock[i]);

  // 若未缓存,则从哦你当前桶中查找
  acquire(&bcache.biglock);
  acquire(&bcache.lock[i]);

  for (b = bcache.head[i].next; b != &bcache.head[i]; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[i]);
      release(&bcache.biglock);
      acquiresleep(&b->lock);
      return b;  // 返回已缓存的块，并增加引用计数
    }
  }

  // 从当前桶中查找最近最少使用的块。
  for (b = bcache.head[i].next; b != &bcache.head[i]; b = b->next) {
    if (b->refcnt == 0 && (b2 == 0 || b->lastuse < min_ticks)) {
      min_ticks = b->lastuse;
      b2 = b;
    }
  }
  if (b2) {
    b2->dev = dev;
    b2->blockno = blockno;
    b2->refcnt++;
    b2->valid = 0;
    release(&bcache.lock[i]);
    release(&bcache.biglock);
    acquiresleep(&b2->lock);
    return b2;  // 返回选择的块（LRU 或未使用的），并增加引用计数
  }

  // 若当前桶中找不到,则从其他桶中查找块。
  for (int k = 1; k < NBUCKET;++k) {
    int j = bufhash(i + k);
    acquire(&bcache.lock[j]);
    for (b = bcache.head[j].next; b != &bcache.head[j]; b = b->next) {
      if (b->refcnt == 0 && (b2 == 0 || b->lastuse < min_ticks)) {
        min_ticks = b->lastuse;
        b2 = b;
      }
    }
    if(b2) {
      b2->dev = dev;
      b2->refcnt++;
      b2->valid = 0;
      b2->blockno = blockno;
      // 从原始桶中移除块。
      b2->next->prev = b2->prev;
      b2->prev->next = b2->next;
      release(&bcache.lock[j]);
      // 添加块到当前桶。
      b2->next = bcache.head[i].next;
      b2->prev = &bcache.head[i];
      bcache.head[i].next->prev = b2;
      bcache.head[i].next = b2;
      release(&bcache.lock[i]);
      release(&bcache.biglock);
      acquiresleep(&b2->lock);
      return b2;  // 返回选择的块，从不同桶中，并增加引用计数
    }
    release(&bcache.lock[j]);
  }
  release(&bcache.lock[i]);
  release(&bcache.biglock);
  // 如果没有找到合适的块，则触发 panic
  panic("bget: 没有可用的块");  
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int i = bufhash(b->blockno); // 计算块号的哈希桶索引

  acquire(&bcache.lock[i]); // 获取对应的缓冲区桶锁
  b->refcnt--; // 减少引用计数
  if (b->refcnt == 0) {
    b->lastuse = ticks; // 更新最近使用时间

    // 如果没有其他进程等待该缓冲区，将其移动到链表头
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[i].next;
    b->prev = &bcache.head[i];
    bcache.head[i].next->prev = b;
    bcache.head[i].next = b;
  }

  release(&bcache.lock[i]); // 释放缓冲区桶锁
}

void
bpin(struct buf *b) {
  int i = bufhash(b->blockno); // 计算块号的哈希桶索引
  acquire(&bcache.lock[i]); // 获取对应的缓冲区桶锁
  b->refcnt++; // 增加引用计数
  release(&bcache.lock[i]); // 释放缓冲区桶锁
}

void
bunpin(struct buf *b) {
  int i = bufhash(b->blockno); // 计算块号的哈希桶索引
  acquire(&bcache.lock[i]); // 获取对应的缓冲区桶锁
  b->refcnt--; // 减少引用计数
  release(&bcache.lock[i]); // 释放缓冲区桶锁
}

