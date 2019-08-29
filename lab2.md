# lab2
## 【实验题目】
实验 3 物理内存管理
## 【实验目的】
• 理解基于段页式内存地址的转换机制  
• 理解页表的建立和使用方法  
• 理解物理内存的管理方法  
## 【实验要求】
• 练习 0 ：填写已有实验  
• 练习 1 ：实现 first fit 连续物理内存分配算法（需要编程）  
• 练习 2 ：实现寻找虚拟地址对应的页表项（需要编程  
• 练习 3 ：释放某虚地址所在的页并取消对应二级页表项的映射（需要编程  
## 【实验方案】 
 环境是ubuntu14，使用的工具主要是 sublime-text3,VSCode  
### 练习 0：
因为代码量较少，直接复制过即可。 
### 练习 1： 
为了能理解练习 1 的注释与代码，找到练习 1 一些数据与函数的定义是必要的。   
Page: 
```c
struct Page {
    int ref;                        // page frame's reference counter
    uint32_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // the num of free block, used in first fit pm manager
    list_entry_t page_link;         // free list link
};
```
其包含一个被引用的数量 ref，一个表示状态的变量 flags，一个表示空闲页数量的 property,以及连接空闲列表的 page_link。   
list_entry_t: 
```c
struct list_entry {
    struct list_entry *prev, *next;
};	 
``` 
可以看到 list_entry_t 是 list_entry 的一个重定义，其是一个双向链表的结点，因此可以知道空闲页表中的各页是以双向链表的形式存在的。   
首先要修改的函数是 default_init_memmap, 这个函数的作用是对一个空闲空间进行初始化，原有代码实现了大部分的功能，主要缺陷在于遍历中间的空闲页没有被加入到空闲页表中，在注释最后提到可以用 lis_add_before(&free, &(p->page_link))来实现这一功能（双向链表） 另一个缺陷在于中间页的 flags 处未被设为 PG_property(其值为 1)，使用的函数是  
SetPageProperty 
```c
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
```
因而改动后的代码为  
```c
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
		SetPageProperty(p);       //p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
        set_page_ref(p, 0);
	    list_add_before(&free_list, &(p->page_link));//We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
    }
    base->property = n;
    nr_free += n;
}
``` 
下一个需要被修改的函数是default_alloc_pages，其作用在于寻找第一个能够满足需求的空闲块（First-fit），涉及的函数与结构在上一函数已列出。主要思想是先顺序查找，直到查找完或找到第一个满足条件的空闲块，当找到后，就将该块截断取出。因此可以明显的看出原代码的问题，当块被找到之后，只取出了第一页，而其他页并没有被取出，且没有被设为保留页，所以按照这个思路进行改动。
```c
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    list_entry_t *nextList;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL){
        int temp;
        //pick and unlink all pages selected
        struct Page *tempPage;
        for (temp = 0; temp < n; temp++){
            nextList = list_next(le);
            tempPage = le2page(le, page_link);
            SetPageReserved(tempPage);
            ClearPageProperty(tempPage);
            list_del(le);
            le = nextList;
        }
        if (page->property>n)
            (le2page(le,page_link))->property = page->property - n;
        ClearPageProperty(page);
        SetPageReserved(page);
        nr_free -= n;
    }
    return page;
}
```  
最后一个需要修改的函数是 default_free_pages，这个函数的用处是将使用完的页进行回收。原代码的缺陷主要在三点，一是插入链表是并没有找到 base 相对应的位置，
二是没有把页插入到空闲页表中，三是合并不合理。因此对代码的改动如下 
```c
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(PageReserved(base));
    list_entry_t *le = &free_list;
    struct Page *p;
    //search correct position
    while((le=list_next(le)) != &free_list){
        p = le2page(le, page_link);
        if(p>base)
            break;
    }
    //insert all pages and clear flag
    for(p = base; p<base+n; p++){
        p->flags = 0;
        set_page_ref(p, 0);
        ClearPageReserved(p);
        ClearPageProperty(p);
        list_add_before(le, &(p->page_link));
    }
    base->property = n;
    SetPageProperty(base);
    //combine with high address
    p = le2page(le,page_link) ;
    if(base+n == p){
        base->property += p->property;
        p->property = 0;
    }
    //combine with low address
    le = list_prev(&(base->page_link));
    p = le2page(le, page_link);
    if(p==base-1){
        while (le != &free_list) {
            if (p->property) {
                p->property += base->property;
                base->property = 0;
                break;
            }
            le = list_prev(le);
            p = le2page(le, page_link);
        }
    }
    nr_free += n;
    return ;
}
```
### 练习 2： 
练习 2 要补充的函数是 get_pte，其作用是通过二级虚地址寻找内核虚地址  
代码逻辑为：首先获取页表，如果页表不存在，则判断是否需要创建一个新的页表，若不需要或创建失败则直接返回，剩下的就是对新建页表进行设置。最后将页表返回  
```c
    pde_t *pdep = &pgdir[PDX(la)];                      // (1) find page directory entry
    if (!(*pdep & PTE_P)){                              // (2) check if entry is not present 
        struct Page *page;
        if (create){                                    // (3) check if creating is needed, then alloc page for page table
            if((page = alloc_page())==NULL)
                return NULL;
        }else
            return NULL;
        set_page_ref(page, 1);                          // (4) set page reference
        uintptr_t addr = page2pa(page);                 // (5) get linear address of page
        memset(KADDR(addr), 0, PGSIZE);                  // (6) clear page content using memset
        *pdep = addr | PTE_U | PTE_W | PTE_P;             // (7) set page directory entry's permission
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];// (8) return page table entry
```  	
根据提示使用 PDX 获得虚拟地址在 pgdir 中的索引来获取需要的页表，PTE_P 是否存在的标志位，将获得的页表与其相与判断是否存在。最后返回时需要使用到 KADDR 来访问物理地址。  
   
请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中每个组成部分的含义以及对 ucore 而言的潜在用处。 
Ucore 中对页目录项和页表项的表示如下 
```c
/* page directory and page table constants */
#define NPDEENTRY       1024                    // page directory entries per page directory
#define NPTEENTRY       1024                    // page table entries per page table

#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)
#define PTSIZE          (PGSIZE * NPTEENTRY)    // bytes mapped by a page directory entry
#define PTSHIFT         22                      // log2(PTSIZE)

#define PTXSHIFT        12                      // offset of PTX in a linear address
#define PDXSHIFT        22                      // offset of PDX in a linear address

/* page table/directory entry flags */
#define PTE_P           0x001                   // Present
#define PTE_W           0x002                   // Writeable
#define PTE_U           0x004                   // User
#define PTE_PWT         0x008                   // Write-Through
#define PTE_PCD         0x010                   // Cache-Disable
#define PTE_A           0x020                   // Accessed
#define PTE_D           0x040                   // Dirty
#define PTE_PS          0x080                   // Page Size
#define PTE_MBZ         0x180                   // Bits must be zero
#define PTE_AVAIL       0xE00                   // Available for software use
                                                // The PTE_AVAIL bits aren't used by the kernel or interpreted by the
                                                // hardware, so user processes are allowed to set them arbitrarily.
```
页目录项 
![](picture\lab2-1.png)
页表项 
![](picture\lab2-2.png)  
```
因而 P TE_AVAIL 对应 P DE 和 P TE 的 A vail
PTE_PS 对应 P DE 的 Page Size
PTE_MBZ 对应 P DE 和 PTE 的 0
PTE_A 对应 PDE 和 PTE 的 A ccessed
PTE_D 对应 P TE 的 Dirty
PTE_PCD 对应 P DE 和 P TE 的 Ca che Disabled
PTE_PWT 对应 P DE 和 P TE 的 Write Through
PTE_U 对应 P DE 和 P TE 的 User S upervisor
PTE_W 对应 P DE 和 P T 的 Read W rite
PTE_P 对应 P DE 和 P T 的 Pre sent
```
如果 ucore 执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情?  
首先保存引发异常时的寄存器现场，陷入操作系统发现页错误并由进入异常处理程序，处理完成后恢复上下文。 
### 练习 3：  	
练习 3 的目的是要取消一个页的页表映射，当其引用次数为 0 时，也需要将此表释放.  
因此只需要先判断页表是否存在，然后对页表的引用减一，再判断是否需要释放该页即可。根据注释编写代码，再参考练习 2   
```c
    if (*ptep & PTE_P) {                        //(1) check if this page table entry is present
        struct Page *page = pte2page(*ptep);    //(2) find corresponding page to pte
        page_ref_dec(page);                     //(3) decrease page reference
        if (page->ref == 0) {    
            free_page(page);                    //(4) and free this page when page reference reachs 0
        }
        *ptep = 0;                              //(5) clear second page table entry
        tlb_invalidate(pgdir, la);              //(6) flush tlb
    }
```
重要知识点： 
空闲页表的双向链表存在形式  
First-fit 的实现方法    
使用页的回收与合并  
页表的查询   
页表的释放等 
## 【实验过程】
![](picture\lab2-3.jpg)