#include "mmu.h"
#include "pmap.h"
#include "printf.h"
#include "env.h"
#include "error.h"

/* These variables are set by mips_detect_memory() */
u_long maxpa;            /* Maximum physical address */
u_long npage;            /* Amount of memory(in pages) */
u_long basemem;          /* Amount of base memory(in bytes) */
u_long extmem;           /* Amount of extended memory(in bytes) */

Pde *boot_pgdir;

struct Page *pages;
static u_long freemem;

static struct Page_list page_free_list;	/* Free list of physical pages */

/* Exercise 2.1 */
/* Overview:
   Initialize basemem and npage.
   Set basemem to be 64MB, and calculate corresponding npage value.*/
void mips_detect_memory()
{
	/* Step 1: Initialize basemem.
	 * (When use real computer, CMOS tells us how many kilobytes there are). */
	basemem = 1 << 26;  // set basemem to be 64MB
	maxpa = 1 << 26;  // set maxpa to Maximum physical address
	extmem = 0;  // no extensions in lab2
	// Step 2: Calculate corresponding npage value.
	npage = basemem >> PGSHIFT;  // calculate corresponding npage value

	printf("Physical memory: %dK available, ", (int)(maxpa / 1024));
	printf("base = %dK, extended = %dK\n", (int)(basemem / 1024),
			(int)(extmem / 1024));
}

/* Overview:
   Allocate `n` bytes physical memory with alignment `align`, if `clear` is set, clear the
   allocated memory.
   This allocator is used only while setting up virtual memory system.

   Post-Condition:
   If we're out of memory, should panic, else return this address of memory we have allocated.*/
static void *alloc(u_int n, u_int align, int clear)
{
	extern char end[];  // lab1中的0x80400000，映射到0x00400000.因此我们能管理的地址空间是[0x00400000, maxpa - 1],对应的虚拟空间[0x80400000, 0x83ffffff]
	u_long alloced_mem;

	/* Initialize `freemem` if this is the first time. The first virtual address that the
	 * linker did *not* assign to any kernel code or global variables. */
	if (freemem == 0) {
		freemem = (u_long)end;  // 全局变量freemem初始值为0，第一次调用时初始化为end，表示[0x80400000, freemem - 1]的虚拟地址都被分配了
	}

	/* Step 1: Round up `freemem` up to be aligned properly */
	freemem = ROUND(freemem, align);  // 找到最小的符合条件的虚拟地址

	/* Step 2: Save current value of `freemem` as allocated chunk. */
	alloced_mem = freemem;  // alloced_mem为初始化虚拟地址

	/* Step 3: Increase `freemem` to record allocation. */
	freemem = freemem + n;  // freemem + n表示这一段被占用了

	/* Check if we're out of memory. If we are, PANIC !! */
	if (PADDR(freemem) >= maxpa) {  // 判断freemem对应的物理地址是否超出最大物理地址
		panic("out of memory\n");
		return (void *)-E_NO_MEM;
	}

	/* Step 4: Clear allocated chunk if parameter `clear` is set. */
	if (clear) {
		bzero((void *)alloced_mem, n);  // 如果clear，就将当前大小为n的块清零
	}

	/* Step 5: return allocated chunk. */
	return (void *)alloced_mem;  // 返回初始虚拟地址
}

/* Exercise 2.6 */
/* Overview:
   Get the page table entry for virtual address `va` in the given
   page directory `pgdir`.
   If the page table is not exist and the parameter `create` is set to 1,
   then create it.
   返回一级页表基地址 pgdir 对应的两级页表结构中，va 这个虚拟地址所在的二级页表项*/
static Pte *boot_pgdir_walk(Pde *pgdir, u_long va, int create)
{
	// 一级页表基地址 pgdir  虚拟地址 va  新建 create
	// 功能：它返回一级页表基地址 pgdir 对应的两级页表结构中，va 这个虚拟地址所在的二级页表项，如果 create 不为 0 且对应的二级页表不存在则会使用 alloc 函数分配一页物理内存用于存放。
	// 查找页表项用的都是虚拟地址，页表项中存储的都是物理页号+标志位
	
	/* Step 1: Get the corresponding page directory entry and page table. */
	/* Hint: Use KADDR and PTE_ADDR to get the page table from page directory
	 * entry value. */
	Pde *pgdir_entry = pgdir + PDX(va); // 一级页表基地址(pgdir)+一级页表偏移量(PDX(va)) => 一级页表项*pgdir_entry
										// *pgdir_entry存储的是20位物理页号+12位权限位

    // check whether the page table exists
	/* Step 2: If the corresponding page table is not exist and parameter `create`
	* is set, create one. And set the correct permission bits for this new page
	* table. */
    if ((*pgdir_entry & PTE_V) == 0) {
        if (create) {  // 如果没有二级页表且create = 1，则新建一个二级页表
			*pgdir_entry = PADDR(alloc(BY2PG, BY2PG, 1));  // 在kseg0区分配一页内存，转化为物理地址赋值给*pgdir_entry
			*pgdir_entry = *pgdir_entry | PTE_V | PTE_R;  // 标记权限位
			/**
			 * use `alloc` to allocate a page for the page table
			 * set permission: `PTE_V | PTE_R`
			 * hint: `PTE_V` <==> valid ; `PTE_R` <==> writable
			 */
		} else return 0; // exception
    }

    // return the address of entry of page table
	/* Step 3: Get the page table entry for `va`, and return it. */
	// PTE_ADDR(*pgdir_entry) 将一级页表项内容的后12位清零，得到物理页号
	// KADDR(PTE_ADDR(*pgdir_entry)) 转化成kseg0区的虚拟地址，二级页表项的基地址
	// ((Pte *)(KADDR(PTE_ADDR(*pgdir_entry)))) + PTX(va)  二级页表项的基地址＋偏移，得到二级页表项
    return ((Pte *)(KADDR(PTE_ADDR(*pgdir_entry)))) + PTX(va);  // 找到二级页表项
}

/* Exercise 2.7 */
/*Overview:
  Map [va, va+size) of virtual address space to physical [pa, pa+size) in the page
  table rooted at pgdir.
  Use permission bits `perm | PTE_V` for the entries.

  Pre-Condition:
  Size is a multiple of BY2PG.
  将一级页表基地址 pgdir 对应的两级页表结构做区间地址映射，将虚拟地址区间 [va, va + size − 1] 映射到物理地址区间 [pa, pa + size − 1]（二级页表和物理地址做映射）*/
void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm)
{
	/* Step 1: Check if `size` is a multiple of BY2PG. */
	size = ROUND(size, BY2PG);

	/* Step 2: Map virtual address space to physical address. */
	/* Hint: Use `boot_pgdir_walk` to get the page table entry of virtual address `va`. */
    int i;
    Pte *pgtable_entry;
    for (i = 0; i < size; i += BY2PG) {
        /* Step 1. use `boot_pgdir_walk` to "walk" the page directory */
        pgtable_entry = boot_pgdir_walk(pgdir, va + i, 1);  // 调用boot_pgdir_walk，查找虚拟地址 va + i 对应的二级页表项
        /* Step 2. fill in the page table */
        *pgtable_entry = (PTE_ADDR(pa))| perm | PTE_V;  // 给二级页表项赋值为物理页号+标志位  PTE_ADDR(pa)得到物理页号，再并上标志位
	}
}

/* Overview:
   Set up two-level page table.

Hint:
You can get more details about `UPAGES` and `UENVS` in include/mmu.h. */
void mips_vm_init()
{
	extern char end[];
	extern int mCONTEXT;
	extern struct Env *envs;

	Pde *pgdir;
	u_int n;

	/* Step 1: Allocate a page for page directory(first level page table). */
	pgdir = alloc(BY2PG, BY2PG, 1);  // 为内核一级页表分配一页物理内存
	printf("to memory %x for struct page directory.\n", freemem);
	mCONTEXT = (int)pgdir;

	boot_pgdir = pgdir;

	/* Step 2: Allocate proper size of physical memory for global array `pages`,
	 * for physical memory management. Then, map virtual address `UPAGES` to
	 * physical address `pages` allocated before. In consideration of alignment,
	 * you should round up the memory size before map. */
	pages = (struct Page *)alloc(npage * sizeof(struct Page), BY2PG, 1);  // 为物理内存管理所用到的Page结构体分配物理内存
	printf("to memory %x for struct Pages.\n", freemem);
	n = ROUND(npage * sizeof(struct Page), BY2PG);
	boot_map_segment(pgdir, UPAGES, n, PADDR(pages), PTE_R);  // 将分配的空间映射到上述的内核页表中，对应的起始虚拟地址为 UPAGS

	/* Step 3, Allocate proper size of physical memory for global array `envs`,
	 * for process management. Then map the physical address to `UENVS`. */
	envs = (struct Env *)alloc(NENV * sizeof(struct Env), BY2PG, 1);  // 为进程管理用到的env结构体分配物理内存
	n = ROUND(NENV * sizeof(struct Env), BY2PG);
	boot_map_segment(pgdir, UENVS, n, PADDR(envs), PTE_R); // 将分配的空间映射到上述的内核页表中，对应的起始虚拟地址为 UENVS

	printf("pmap.c:\t mips vm init success\n");
}

/* Exercise 2.3 */
/*Overview:
  Initialize page structure and memory free list.
  The `pages` array has one `struct Page` entry per physical page. Pages
  are reference counted, and free pages are kept on a linked list.
Hint:
Use `LIST_INSERT_HEAD` to insert something to list.*/
void page_init(void)
{
	/* Step 1: Initialize page_free_list. */
	/* Hint: Use macro `LIST_INIT` defined in include/queue.h. */
	LIST_INIT(&page_free_list);

	/* Step 2: Align `freemem` up to multiple of BY2PG. */
	ROUND(freemem, BY2PG);

	/* Step 3: Mark all memory blow `freemem` as used(set `pp_ref`
	 * filed to 1) */

	// pages是指针，当成数组用；current_page是指向Page的一个指针
	struct Page *current_page;
	// current_page从第一个物理页面开始，遍历page，将page的引用次数设为1，直到current_page对应的kseg0虚拟地址大于等于freemem
	for (current_page = &pages[0]; page2kva(current_page) < freemem; current_page++){
		current_page->pp_ref = 1;
	}

	/* Step 4: Mark the other memory as free. */
	// current_page从freemem对应的物理页面开始(将kseg0虚拟地址freemem经PADDR转化为物理地址，用PPN求物理页号即pages数组的index)开始，
	// 遍历page，将page的引用次数设为0,并插入page_free_list链表中，到current_page的物理页号大于等于最大物理页号为止
	// 判断条件也可以为page2pa(current_page) < basemem ??
	for (current_page = &pages[PPN(PADDR(freemem))]; page2ppn(current_page) < npage;current_page++){
		if(page2kva(current_page) != TIMESTACK) {
			current_page->pp_ref = 0;
			LIST_INSERT_HEAD(&page_free_list, current_page, pp_link);  // pp_link is type LIST_ENTRY(Page)
		}
	}
}

/* Exercise 2.4 */
/*Overview:
  Allocates a physical page from free memory, and clear this page.

  Post-Condition:
  If failing to allocate a new page(out of memory(there's no free page)),
  return -E_NO_MEM.
  Else, set the address of allocated page to *pp, and return 0.

Note:
DO NOT increment the reference count of the page - the caller must do
these if necessary (either explicitly or via page_insert).

Hint:
Use LIST_FIRST and LIST_REMOVE defined in include/queue.h .*/
int page_alloc(struct Page **pp) {
	struct Page *tmp;
	/* Step 1: Get a page from free memory. If fail, return the error code.*/
	if (LIST_EMPTY(&page_free_list)) {
		return -E_NO_MEM;
	}
	// negative return value indicates exception.

	tmp = LIST_FIRST(&page_free_list);

	LIST_REMOVE(tmp, pp_link);

	/* Step 2: Initialize this page.
	* Hint: use `bzero`. */
	bzero(page2kva(tmp), BY2PG);

	*pp = tmp;
	return 0;
	// zero indicates success.
}

/* Exercise 2.5 */
/*Overview:
  Release a page, mark it as free if it's `pp_ref` reaches 0.
Hint:
When you free a page, just insert it to the page_free_list.*/
void page_free(struct Page *pp)
{
	/* Step 1: If there's still virtual address referring to this page, do nothing. */
	if((pp->pp_ref) > 0){
		return;
	}
	/* Step 2: If the `pp_ref` reaches 0, mark this page as free and return. */
	if ((pp->pp_ref) == 0) {
		LIST_INSERT_HEAD(&page_free_list, pp, pp_link);
		return;
	}

	/* If the value of `pp_ref` is less than 0, some error must occurr before,
	 * so PANIC !!! */
	panic("cgh:pp->pp_ref is less than zero\n");
}

/* Exercise 2.8 */
/*Overview:
  Given `pgdir`, a pointer to a page directory, pgdir_walk returns a pointer
  to the page table entry (with permission PTE_R|PTE_V) for virtual address 'va'.

  Pre-Condition:
  The `pgdir` should be two-level page table structure.

  Post-Condition:
  If we're out of memory, return -E_NO_MEM.
  Else, we get the page table entry successfully, store the value of page table
  entry to *ppte, and return 0, indicating success.

Hint:
We use a two-level pointer to store page table entry and return a state code to indicate
whether this function execute successfully or not.
This function has something in common with function `boot_pgdir_walk`.
它返回一级页表基地址 pgdir 对应的两级页表结构中，va 这个虚拟地址所在的二级页表项，不存在二级页表项时可以申请 */
int pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte)
{
	/* Step 1: Get the corresponding page directory entry and page table. */
	Pde *pgdir_entry = pgdir + PDX(va);
    struct Page *page;
    int ret;
    
    // check whether the page table exists
	/* Step 2: If the corresponding page table is not exist(valid) and parameter `create`
	 * is set, create one. And set the correct permission bits for this new page table.
	 * When creating new page table, maybe out of memory. */
    if ((*pgdir_entry & PTE_V) == 0) {
        if (create) {
            if ((ret = page_alloc(&page)) < 0) return ret;
            *pgdir_entry = (page2pa(page)) | PTE_V | PTE_R;  // 如果二级页表不存在，create不为0，则新申请一个二级页表
			page->pp_ref++;  // 物理页面被引用次数+1
		} else {
			*ppte = 0;  // 如果不存在且不create，则将pte指针设为0。在page_insert中会用到
            return 0;
		}
	}

	/* Step 3: Set the page table entry to `*ppte` as return value. */
    *ppte = ((Pte *)(KADDR(PTE_ADDR(*pgdir_entry)))) + PTX(va);
    // *ppte = ((Pte *)(page2kva(page))) + PTX(va); page就是给二级页表申请的，它的虚拟地址就是二级页表的首地址，但仅当page是新申请时候成立
    return 0;
}

/* Exercise 2.9 */
/*Overview:
  Map the physical page 'pp' at virtual address 'va'.
  The permissions (the low 12 bits) of the page table entry should be set to 'perm|PTE_V'.

  Post-Condition:
  Return 0 on success
  Return -E_NO_MEM, if page table couldn't be allocated

Hint:
If there is already a page mapped at `va`, call page_remove() to release this mapping.
The `pp_ref` should be incremented if the insertion succeeds.
va所在的二级页表项和物理页面pp做映射，函数里将 pp_ref + 1 了
*/
int page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm)
{
	Pte *pgtable_entry;
    int ret;

    perm = perm | PTE_V;

    // Step 0. check whether `va` is already mapping to `pa`
    pgdir_walk(pgdir, va, 0 /* for check */, &pgtable_entry);  // pgtable_entry是va当前对应的二级页表项
    if (pgtable_entry != 0 && (*pgtable_entry & PTE_V) != 0) {
        // check whether `va` is mapping to another physical frame
        if (pa2page(*pgtable_entry) != pp) {  // 如果当前va和pp没有对应上，即二级页表项存储的地址不是pa
            page_remove(pgdir, va); // unmap it!  // 解绑
        } else {  // 如果对应上了，将tlb中虚拟地址的映射删除(因为va和pa的对应关系已经改变)
            tlb_invalidate(pgdir, va);              // <~~
            *pgtable_entry = page2pa(pp) | perm;    // update the permission
            return 0;
        }
    }
    tlb_invalidate(pgdir, va);                      // <~~
    /* Step 1. use `pgdir_walk` to "walk" the page directory */
    if ((ret = pgdir_walk(pgdir, va, 1, &pgtable_entry)) < 0)
        return ret; // exception
    /* Step 2. fill in the page table */
    *pgtable_entry = (PTE_ADDR(page2pa(pp))) | perm;
    pp->pp_ref++;
    return 0;
}

/*Overview:
	Look up the Page that virtual address `va` map to.

	Post-Condition:
  	Return a pointer to corresponding Page, and store it's page table entry to *ppte.
	If `va` doesn't mapped to any Page, return NULL.
	返回一级页表基地址 pgdir 对应的两级页表结构中 va 这一虚拟地址映射对应的物理页面对应的内存控制块，同时将 ppte 指向的空间设为对应的二级页表项地址。
  	该函数即返回查找到的物理页面，又能将一个指针指向二级页表项的地址
	可用于查找虚拟地址va是否在页目录pgdir表示的区域中 */
struct Page *page_lookup(Pde *pgdir, u_long va, Pte **ppte)
{
	struct Page *ppage;
	Pte *pte;

	/* Step 1: Get the page table entry. */
	pgdir_walk(pgdir, va, 0, &pte);

	/* Hint: Check if the page table entry doesn't exist or is not valid. */
	if (pte == 0) {
		return 0;
	}
	if ((*pte & PTE_V) == 0) {
		return 0;    //the page is not in memory.
	}

	/* Step 2: Get the corresponding Page struct. */

	/* Hint: Use function `pa2page`, defined in include/pmap.h . */
	ppage = pa2page(*pte);
	if (ppte) {
		*ppte = pte;
	}

	return ppage;
}

// Overview:
// 	Decrease the `pp_ref` value of Page `*pp`, if `pp_ref` reaches to 0, free this page.
void page_decref(struct Page *pp) {
	if(--pp->pp_ref == 0) {
		page_free(pp);
	}
}

// Overview:
// 	Unmaps the physical page at virtual address `va`.
// 删除一级页表基地址 pgdir 对应的两级页表结构中 va 这一虚拟地址对物理地址的映射，如果存在这样的映射，那么对应物理页面的引用次数会减少。
void page_remove(Pde *pgdir, u_long va)
{
	Pte *pagetable_entry;
	struct Page *ppage;

	/* Step 1: Get the page table entry, and check if the page table entry is valid. */

	ppage = page_lookup(pgdir, va, &pagetable_entry);

	if (ppage == 0) {
		return;
	}

	/* Step 2: Decrease `pp_ref` and decide if it's necessary to free this page. */

	/* Hint: When there's no virtual address mapped to this page, release it. */
	ppage->pp_ref--;
	if (ppage->pp_ref == 0) {
		page_free(ppage);
	}

	/* Step 3: Update TLB. */
	*pagetable_entry = 0;
	tlb_invalidate(pgdir, va);
	return;
}

// Overview:
// 	Update TLB.
void tlb_invalidate(Pde *pgdir, u_long va)
{
	if (curenv) {
		tlb_out(PTE_ADDR(va) | GET_ENV_ASID(curenv->env_id));
	} else {
		tlb_out(PTE_ADDR(va));
	}
}

void physical_memory_manage_check(void)
{
	struct Page *pp, *pp0, *pp1, *pp2;
	struct Page_list fl;
	int *temp;

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert(page_alloc(&pp0) == 0);
	assert(page_alloc(&pp1) == 0);
	assert(page_alloc(&pp2) == 0);

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);



	// temporarily steal the rest of the free pages
	fl = page_free_list;
	// now this page_free list must be empty!!!!
	LIST_INIT(&page_free_list);
	// should be no free memory
	assert(page_alloc(&pp) == -E_NO_MEM);

	temp = (int*)page2kva(pp0);
	//write 1000 to pp0
	*temp = 1000;
	// free pp0
	page_free(pp0);
	printf("The number in address temp is %d\n",*temp);

	// alloc again
	assert(page_alloc(&pp0) == 0);
	assert(pp0);

	// pp0 should not change
	assert(temp == (int*)page2kva(pp0));
	// pp0 should be zero
	assert(*temp == 0);

	page_free_list = fl;
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);
	struct Page_list test_free;
	struct Page *test_pages;
	test_pages= (struct Page *)alloc(10 * sizeof(struct Page), BY2PG, 1);
	LIST_INIT(&test_free);
	//LIST_FIRST(&test_free) = &test_pages[0];
	int i,j=0;
	struct Page *p, *q;
	//test inert tail
	for(i=0;i<10;i++) {
		test_pages[i].pp_ref=i;
		//test_pages[i].pp_link=NULL;
		//printf("0x%x  0x%x\n",&test_pages[i], test_pages[i].pp_link.le_next);
		LIST_INSERT_TAIL(&test_free,&test_pages[i],pp_link);
		//printf("0x%x  0x%x\n",&test_pages[i], test_pages[i].pp_link.le_next);

	}
	p = LIST_FIRST(&test_free);
	int answer1[]={0,1,2,3,4,5,6,7,8,9};
	assert(p!=NULL);
	while(p!=NULL)
	{
		//printf("%d %d\n",p->pp_ref,answer1[j]);
		assert(p->pp_ref==answer1[j++]);
		//printf("ptr: 0x%x v: %d\n",(p->pp_link).le_next,((p->pp_link).le_next)->pp_ref);
		p=LIST_NEXT(p,pp_link);

	}
	// insert_after test
	int answer2[]={0,1,2,3,4,20,5,6,7,8,9};
	q=(struct Page *)alloc(sizeof(struct Page), BY2PG, 1);
	q->pp_ref = 20;

	//printf("---%d\n",test_pages[4].pp_ref);
	LIST_INSERT_AFTER(&test_pages[4], q, pp_link);
	//printf("---%d\n",LIST_NEXT(&test_pages[4],pp_link)->pp_ref);
	p = LIST_FIRST(&test_free);
	j=0;
	//printf("into test\n");
	while(p!=NULL){
		//      printf("%d %d\n",p->pp_ref,answer2[j]);
		assert(p->pp_ref==answer2[j++]);
		p=LIST_NEXT(p,pp_link);
	}



	printf("physical_memory_manage_check() succeeded\n");
}


void page_check(void)
{
	struct Page *pp, *pp0, *pp1, *pp2;
	struct Page_list fl;

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert(page_alloc(&pp0) == 0);
	assert(page_alloc(&pp1) == 0);
	assert(page_alloc(&pp2) == 0);

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	// now this page_free list must be empty!!!!
	LIST_INIT(&page_free_list);

	// should be no free memory
	assert(page_alloc(&pp) == -E_NO_MEM);

	// there is no free memory, so we can't allocate a page table
	assert(page_insert(boot_pgdir, pp1, 0x0, 0) < 0);

	// free pp0 and try again: pp0 should be used for page table
	page_free(pp0);
	assert(page_insert(boot_pgdir, pp1, 0x0, 0) == 0);
	assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));

	printf("va2pa(boot_pgdir, 0x0) is %x\n",va2pa(boot_pgdir, 0x0));
	printf("page2pa(pp1) is %x\n",page2pa(pp1));

	assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
	assert(pp1->pp_ref == 1);

	// should be able to map pp2 at BY2PG because pp0 is already allocated for page table
	assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// should be no free memory
	assert(page_alloc(&pp) == -E_NO_MEM);

	printf("start page_insert\n");
	// should be able to map pp2 at BY2PG because it's already there
	assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// pp2 should NOT be on the free list
	// could happen in ref counts are handled sloppily in page_insert
	assert(page_alloc(&pp) == -E_NO_MEM);

	// should not be able to map at PDMAP because need free page for page table
	assert(page_insert(boot_pgdir, pp0, PDMAP, 0) < 0);

	// insert pp1 at BY2PG (replacing pp2)
	assert(page_insert(boot_pgdir, pp1, BY2PG, 0) == 0);

	// should have pp1 at both 0 and BY2PG, pp2 nowhere, ...
	assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
	// ... and ref counts should reflect this
	assert(pp1->pp_ref == 2);
	printf("pp2->pp_ref %d\n",pp2->pp_ref);
	assert(pp2->pp_ref == 0);
	printf("end page_insert\n");

	// pp2 should be returned by page_alloc
	assert(page_alloc(&pp) == 0 && pp == pp2);

	// unmapping pp1 at 0 should keep pp1 at BY2PG
	page_remove(boot_pgdir, 0x0);
	assert(va2pa(boot_pgdir, 0x0) == ~0);
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp2->pp_ref == 0);

	// unmapping pp1 at BY2PG should free it
	page_remove(boot_pgdir, BY2PG);
	assert(va2pa(boot_pgdir, 0x0) == ~0);
	assert(va2pa(boot_pgdir, BY2PG) == ~0);
	assert(pp1->pp_ref == 0);
	assert(pp2->pp_ref == 0);

	// so it should be returned by page_alloc
	assert(page_alloc(&pp) == 0 && pp == pp1);

	// should be no free memory
	assert(page_alloc(&pp) == -E_NO_MEM);

	// forcibly take pp0 back
	assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));
	boot_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	printf("page_check() succeeded!\n");
}

void pageout(int va, int context)
{
	u_long r;
	struct Page *p = NULL;

	if (context < 0x80000000) {
		panic("tlb refill and alloc error!");
	}

	if ((va > 0x7f400000) && (va < 0x7f800000)) {
		panic(">>>>>>>>>>>>>>>>>>>>>>it's env's zone");
	}

	if (va < 0x10000) {
		panic("^^^^^^TOO LOW^^^^^^^^^");
	}

	if ((r = page_alloc(&p)) < 0) {
		panic ("page alloc error!");
	}

	p->pp_ref++;

	page_insert((Pde *)context, p, VA2PFN(va), PTE_R);
	printf("pageout:\t@@@___0x%x___@@@  ins a page \n", va);
}





