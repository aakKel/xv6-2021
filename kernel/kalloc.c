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



// 为每个物理页增加引用计数，在最后一个pte销毁后释放该物理页
struct pp_ref {
    struct spinlock lock;
    // 最大物理地址 / 每页大小 -> 为每个物理页创建一个映射
    int cnt[PHYSTOP / PGSIZE];

} ref;

int kref_cnt(void *pa) {
    //获取引用计数。
    return ref.cnt[(uint64)pa / PGSIZE];
}

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
  //初始化锁
    initlock(&ref.lock,"ref");
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

    acquire(&ref.lock);
    //引用计数为0才释放.
    if (--ref.cnt[(uint64)pa / PGSIZE] == 0) {
        release(&ref.lock);
        r = (struct run*)pa;

        memset(pa,1,PGSIZE);
        acquire(&kmem.lock);
        r -> next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    } else {
        release(&ref.lock);
    }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist; // 获取内存
    if(r) {
        kmem.freelist = r->next; // 从空闲链表中删除获取的内存
        acquire(&ref.lock);
        ref.cnt[(uint64)r / PGSIZE] = 1;  // 将引用计数初始化为1
        release(&ref.lock);
    }
    release(&kmem.lock);

    if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;

}
