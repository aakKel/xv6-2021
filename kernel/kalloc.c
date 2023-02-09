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
struct ref_stru {
    struct spinlock lock;
    // 最大物理地址 / 每页大小 -> 为每个物理页创建一个映射
    int cnt[PHYSTOP / PGSIZE];
} ref;

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
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
      //kfree 会对页面 -1，这边要先设置为1；
      ref.cnt[(uint64)p/PGSIZE] = 1;
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
//增加4个可调用函数
//1,判断一个页面是否为COW页面
int cowpage(pagetable_t pagetable, uint64 va) {
    if(va >= MAXVA)
        return -1;
    pte_t* pte = walk(pagetable, va, 0);
    if(pte == 0)
        return -1;
    if((*pte & PTE_V) == 0)
        return -1;
    return (*pte & PTE_RTS ? 0 : -1);
}
//2.COW分配器
void* cowalloc(pagetable_t pagetable, uint64 va) {
    if(va % PGSIZE != 0)
        return 0;

    uint64 pa = walkaddr(pagetable, va);  // 获取对应的物理地址
    if(pa == 0)
        return 0;

    pte_t* pte = walk(pagetable, va, 0);  // 获取对应的PTE

    if(krefcnt((char*)pa) == 1) {
        // 只剩一个进程对此物理地址存在引用
        // 则该页面可以写入 且不是共享页
        *pte |= PTE_W;
        *pte &= ~PTE_RTS;
        return (void*)pa;
    } else {
        // 多个进程对物理内存存在引用
        // 需要分配新的页面，并拷贝旧页面的内容
        char* mem = kalloc();
        if(mem == 0)
            return 0;

        // 复制旧页面内容到新页
        memmove(mem, (char*)pa, PGSIZE);

        // 清除PTE_V，否则在mappagges中会判定为remap
        *pte &= ~PTE_V;

        // 为新页面添加映射
        if(mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_RTS) != 0) {
            kfree(mem);
            *pte |= PTE_V;
            return 0;
        }

        // 将原来的物理内存引用计数减1
        kfree((char*)PGROUNDDOWN(pa));
        return mem;
    }
}
//3.获取内存的引用计数。
int krefcnt(void *pa) {
    //获取引用计数。
    return ref.cnt[(uint64)pa / PGSIZE];
}
//4.父进程fork后的子进程使用的是父进程的物理页面，则需要在此物理页面
//上增加引用计数。
int kaddrefcnt(void* pa) { // 放在uvmcopy，增加引用计数
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        return -1;
    acquire(&ref.lock);
    ++ref.cnt[(uint64)pa / PGSIZE];
    release(&ref.lock);
    return 0;
}