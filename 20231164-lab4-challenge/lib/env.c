/* Notes written by Qian Liu <qianlxc@outlook.com>
  If you find any bug, please contact with me.*/

#include <mmu.h>
#include <error.h>
#include <env.h>
#include <kerelf.h>
#include <sched.h>
#include <pmap.h>
#include <printf.h>

struct Env *envs = NULL;        // All environments
struct Env *curenv = NULL;            // the current env
struct Tcb *curtcb = NULL;

static struct Env_list env_free_list;    // Free list
struct Tcb_list tcb_sched_list[2];

extern Pde *boot_pgdir;
extern char *KERNEL_SP;

static u_int asid_bitmap[2] = {0}; // 64


/* Overview:
 *  This function is to allocate an unused ASID
 *
 * Pre-Condition:
 *  the number of running processes should be less than 64
 *
 * Post-Condition:
 *  return the allocated ASID on success
 *  panic when too many processes are running
 */
static u_int asid_alloc() {
    int i, index, inner;
    for (i = 0; i < 64; ++i) {
        index = i >> 5;
        inner = i & 31;
        if ((asid_bitmap[index] & (1 << inner)) == 0) {
            asid_bitmap[index] |= 1 << inner;
            return i;
        }
    }
    panic("too many processes!");
}

/* Overview:
 *  When a process is killed, free its ASID
 *
 * Post-Condition:
 *  ASID is free and can be allocated again later
 */
static void asid_free(u_int i) {
    int index, inner;
    index = i >> 5;
    inner = i & 31;
    asid_bitmap[index] &= ~(1 << inner);
}

/* Overview:
 *  This function is to make a unique ID for every env
 *
 * Pre-Condition:
 *  e should be valid
 *
 * Post-Condition:
 *  return e's envid on success
 */
// idx是十位数，asid是六位数
u_int mkenvid(struct Env *e) {
    u_int idx = e - envs;
    u_int asid = asid_alloc();
    // 正常生成的envid第10位必为1，便于envid2env传入0得到当前结构体功能的实现
    return (asid << (1 + LOG2NENV)) | (1 << LOG2NENV) | idx;
}

/* Overview:
    通过一个进程的envid获取该进程的控制块
 *  Convert an envid to an env pointer.
 *  If envid is 0 , set *penv = curenv; otherwise set *penv = envs[ENVX(envid)];
 *
 * Pre-Condition:
 *  penv points to a valid struct Env *  pointer,
 *  envid is valid, i.e. for the result env which has this envid, 
 *  its status isn't ENV_FREE,
 *  checkperm is 0 or 1.
 *
 * Post-Condition:
 *  return 0 on success,and set *penv to the environment.
 *  return -E_BAD_ENV on error,and set *penv to NULL.
 */
/*** exercise 3.3 ***/
int envid2env(u_int envid, struct Env **penv, int checkperm)
{
    struct Env *e;
    /* Hint: If envid is zero, return curenv.*/
    /* Step 1: Assign value to e using envid. */
    if(envid == 0){
        *penv = curenv;
        return 0;
    }

    e = &envs[ENVX(envid)];  // 根据低十位取env

    if (e->env_id != envid) {  // 低十位相等只能说明在envs中存储的地址一样，不能证明e就是我们要找的进程块
        *penv = 0;
        return -E_BAD_ENV;
    }
    /* Hints:
     *  Check whether the calling env has sufficient permissions
     *    to manipulate the specified env.
     *  If checkperm is set, the specified env
     *    must be either curenv or an immediate child of curenv.
     *  If not, error! */
    /*  Step 2: Make a check according to checkperm. */
    if(checkperm != 0){
        if(e != curenv && e->env_parent_id != curenv->env_id){
            *penv = 0;
            return -E_BAD_ENV;
        }
    }

    *penv = e;
    return 0;
}

/* Overview:
 *  初始化env_free_list env_sched_list
 *  Mark all environments in 'envs' as free and insert them into the env_free_list.
 *  Insert in reverse order,so that the first call to env_alloc() returns envs[0].
 *
 * Hints:
 *  You may use these macro definitions below:
 *      LIST_INIT, LIST_INSERT_HEAD
 */
/*** exercise 3.2 ***/
void
env_init(void)
{
    int i;
    /* Step 1: Initialize env_free_list. */
    LIST_INIT(&env_free_list);
    LIST_INIT(&tcb_sched_list[0]);
    LIST_INIT(&tcb_sched_list[1]);

    /* Step 2: Traverse the elements of 'envs' array,
     *   set their status as free and insert them into the env_free_list.
     * Choose the correct loop order to finish the insertion.
     * Make sure, after the insertion, the order of envs in the list
     *   should be the same as that in the envs array. */
    struct Env *current_env;

    for (current_env = &envs[NENV - 1], i = NENV - 1; i >= 0;current_env--, i--) {
        LIST_INSERT_HEAD(&env_free_list, current_env, env_link);
    }
}


/* Overview:
 *  Initialize the kernel virtual memory layout for 'e'.
 *  Allocate a page directory, set e->env_pgdir and e->env_cr3 accordingly,
 *    and initialize the kernel portion of the new env's address space.
 *  DO NOT map anything into the user portion of the env's virtual address space.
 *  该函数的作用是初始化新进程的地址空间(申请pgdir并赋初值)
 */
/*** exercise 3.4 ***/
static int
env_setup_vm(struct Env *e)
{
    int i, r;
    struct Page *p = NULL;
    Pde *pgdir;

    /* Step 1: Allocate a page for the page directory
     *   using a function you completed in the lab2 and add its pp_ref.
     *   pgdir is the page directory of Env e, assign value for it. */
    if ((r = page_alloc(&p) == -E_NO_MEM)) {
        panic("env_setup_vm - page alloc error\n");
        return r;
    }

    p->pp_ref++;
    pgdir = (Pde*)page2kva(p);
    e->env_pgdir = pgdir;
    e->env_cr3 = PADDR(pgdir);  // 应该是要写的
    
    /* Step 2: Zero pgdir's field before UTOP. */
    for (i = 0; i < PDX(UTOP);i++){
        pgdir[i] = 0;
    }

    /* Step 3: Copy kernel's boot_pgdir to pgdir. */
    /* Hint:
     *  The VA space of all envs is identical above UTOP
     *  (except at UVPT, which we've set below).
     *  See ./include/mmu.h for layout.
     *  Can you use boot_pgdir as a template?
     */
    for (i = PDX(UTOP); i < 1024; i++){
        if(i != PDX(UVPT)){
            pgdir[i] = boot_pgdir[i];
        }
    }

    /* UVPT maps the env's own page table, with read-only permission.*/
    e->env_pgdir[PDX(UVPT)] = e->env_cr3 | PTE_V;  // 这个物理地址就是二级页表所在的物理地址
    return 0;
}

/* Overview:
 *  调用env_setup_vm为进程分配地址空间，分配栈空间，进行进程初始化
 *  Allocate and Initialize a new environment.
 *  On success, the new environment is stored in *new.
 *
 * Pre-Condition:
 *  If the new Env doesn't have parent, parent_id should be zero.
 *  env_init has been called before this function.
 *
 * Post-Condition:
 *  return 0 on success, and set appropriate values of the new Env.
 *  return -E_NO_FREE_ENV on error, if no free env.
 *
 * Hints:
 *  You may use these functions and macro definitions:
 *      LIST_FIRST,LIST_REMOVE, mkenvid (Not All)
 *  You should set some states of Env:
 *      id , status , the sp register, CPU status , parent_id
 *      (the value of PC should NOT be set in env_alloc)
 *  调用env_setup_vm为进程分配地址空间，分配栈空间，进行进程初始化
 */
/*** exercise 3.5 ***/
int
env_alloc(struct Env **new, u_int parent_id)
{
    int r;
    struct Env *e;

    /* Step 1: Get a new Env from env_free_list*/
    if(LIST_EMPTY(&env_free_list)) {
        *new = NULL;
        return -E_NO_FREE_ENV;
    }
    e = LIST_FIRST(&env_free_list);

    /* Step 2: Call a certain function (has been completed just now) to init kernel memory layout for this new Env.
     *The function mainly maps the kernel address to this new Env address. */
    if((r = env_setup_vm(e) != 0)) {  // 初始化地址空间 申请页目录并赋初值
        return r;
    };

    /* Step 3: Initialize every field of new Env with appropriate values.*/
    e->env_id = mkenvid(e);          // id
    e->env_parent_id = parent_id;    // parent_id
    printf("Env alloc succeed! Env id is 0x%x\n", e->env_id);

    /* Step 4: Focus on initializing the sp register and cp0_status of env_tf field, located at this new Env. */
    struct Tcb *t;
    if((r = thread_alloc(e, &t)) != 0){
        return r;
    }

    t->tcb_status = THREAD_RUNNABLE;
    t->tcb_tf.cp0_status = 0x1000100c;
    t->tcb_tf.regs[29] = USTACKTOP - 4 * BY2PG * (t->tcb_id & 0x7);

    /* Step 5: Remove the new Env from env_free_list. */
    LIST_REMOVE(e, env_link);
    *new = e;
    return 0;
}

/* Overview:
 *   把elf文件的各段映射到虚存的恰当位置
 *   This is a call back function for kernel's elf loader.
 *   Elf loader extracts each segment of the given binary image.
 *   Then the loader calls this function to map each segment
 *   at correct virtual address.
 *
 *   `bin_size` is the size of `bin`. `sgsize` is the
 *    segment size in memory.
 *
 * Pre-Condition:
 *   bin can't be NULL.
 *   Hint: va may be NOT aligned with 4KB.
 *
 * Post-Condition:
 *   return 0 on success, otherwise < 0.
 *   把elf的segment段映射到虚存的恰当位置
 */
/*** exercise 3.6 ***/
static int load_icode_mapper(u_long va, u_int32_t sgsize,
                             u_char *bin, u_int32_t bin_size, void *user_data)
{
    struct Env *env = (struct Env *)user_data;
    struct Page *p = NULL;
    u_long i = 0;
	int r;
    int size;
    u_long offset;
    /* Step 1: load all content of bin into memory. */
    offset = va - ROUNDDOWN(va, BY2PG);  // binsize 前面的offset
    if (offset)
    {
        p = page_lookup(env->env_pgdir, va, NULL);  // 最前面的elf段所在的虚拟空间可能已经被申请，需要用page_lookup检查
        if(p == 0) {
            if((r = page_alloc(&p)) != 0){
                return r;
            }
            page_insert(env->env_pgdir, p, va, PTE_R);
        }
        size = MIN(bin_size, BY2PG - offset);
        bcopy((void *)bin, (void *)(page2kva(p) + offset), size);
        i = size;
    }

    while (i < bin_size) {
        /* Hint: You should alloc a new page. */
        if((r = page_alloc(&p)) != 0){
            return r;
        }
        page_insert(env->env_pgdir, p, va + i, PTE_R);
        size = MIN(bin_size - i, BY2PG);
        bcopy((void *)(bin + i), (void *)(page2kva(p)), size);
        i += size;
    }

    /* Step 2: alloc pages to reach `sgsize` when `bin_size` < `sgsize`.
     * hint: variable `i` has the value of `bin_size` now! */
    offset = va + i - ROUNDDOWN(va + i, BY2PG);  // binsize 填完后 后面的offset
    if(offset) {
        p = page_lookup(env->env_pgdir, va + i, NULL);
        if(p == 0) {
            if((r = (page_alloc(&p))) != 0) {
                return r;
            }
            page_insert(env->env_pgdir, p, va + i, PTE_R);
        }
        size = MIN(sgsize - i, BY2PG - offset);
        // bzero((void *)(page2kva(p) + offset), size);
        i += size;
    }

    while (i < sgsize)
    {
        if((r = page_alloc(&p)) != 0){
            return r;
        }
        page_insert(env->env_pgdir, p, va + i, PTE_R);
        size = MIN(sgsize - i, BY2PG);
        // bzero((void *)(page2kva(p)), size);
        i += size;
    }
    return 0;
}
/* Overview:
 * 初始化栈空间，并调用loadelf加载内核
 *  Sets up the the initial stack and program binary for a user process.
 *  This function loads the complete binary image by using elf loader,
 *  into the environment's user memory. The entry point of the binary image
 *  is given by the elf loader. And this function maps one page for the
 *  program's initial stack at virtual address USTACKTOP - BY2PG.
 *
 * Hints:
 *  All mapping permissions are read/write including text segment.
 *  You may use these :
 *      page_alloc, page_insert, page2kva , e->env_pgdir and load_elf.
 * 初始化栈空间，并调用loadelf加载内核
 */
/*** exercise 3.7 ***/
static void
load_icode(struct Env *e, u_char *binary, u_int size)
{
    /* Hint:
     *  You must figure out which permissions you'll need
     *  for the different mappings you create.
     *  Remember that the binary image is an a.out format image,
     *  which contains both text and data.
     */
    struct Page *p = NULL;
    u_long entry_point;
    u_long r;
    u_long perm = PTE_R;  // 可读写

    /* Step 1: alloc a page. */
    if((r = page_alloc(&p)) != 0) {
        return;
    }

    /* Step 2: Use appropriate perm to set initial stack for new Env. */
    /* Hint: Should the user-stack be writable? */
    if((r = page_insert(e->env_pgdir, p, USTACKTOP - BY2PG, perm)) != 0) {
        return;
    }

    /* Step 3: load the binary using elf loader. */
    load_elf(binary, size, &entry_point, (void *)e, load_icode_mapper);
    e->env_threads[0].tcb_status = THREAD_RUNNABLE;
    LIST_INSERT_HEAD(tcb_sched_list, &e->env_threads[0], tcb_sched_link);
    /* Step 4: Set CPU's PC register as appropriate value. */
    e->env_threads[0].tcb_tf.pc = entry_point;   // 设置新进程的pc寄存器为elf文件的入口点
}

/* Overview:
 *  创建进程
 * Allocate a new env with default priority value.
 *
 * Hints:
 *  this function calls the env_create_priority function.
 */
/*** exercise 3.8 ***/
void
env_create(u_char *binary, int size)
{
    struct Env *e;
    /* Step 1: Use env_alloc to alloc a new env. */
    if (env_alloc(&e, 0) != 0) {
        return;
    }
    e->env_threads[0].tcb_pri = 1;
    /* Step 3: Use load_icode() to load the named elf binary,
       and insert it into env_sched_list using LIST_INSERT_HEAD. */
    load_icode(e, binary, size);
}
/* Overview:
 *  Free env e and all memory it uses.
 *  释放进程
 */
void
env_free(struct Env *e)
{
    Pte *pt;
    u_int pdeno, pteno, pa;

    /* Hint: Note the environment's demise.*/
    printf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);

    /* Hint: Flush all mapped pages in the user portion of the address space */
    for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {
        /* Hint: only look at mapped page tables. */
        if (!(e->env_pgdir[pdeno] & PTE_V)) {
            continue;
        }
        /* Hint: find the pa and va of the page table. */
        pa = PTE_ADDR(e->env_pgdir[pdeno]);
        pt = (Pte *)KADDR(pa);
        /* Hint: Unmap all PTEs in this page table. */
        for (pteno = 0; pteno <= PTX(~0); pteno++)
            if (pt[pteno] & PTE_V) {
                page_remove(e->env_pgdir, (pdeno << PDSHIFT) | (pteno << PGSHIFT));
            }
        /* Hint: free the page table itself. */
        e->env_pgdir[pdeno] = 0;
        page_decref(pa2page(pa));
    }
    /* Hint: free the page directory. */
    pa = e->env_cr3;
    e->env_pgdir = 0;
    e->env_cr3 = 0;
    /* Hint: free the ASID */
    asid_free(e->env_id >> (1 + LOG2NENV));
    page_decref(pa2page(pa));
    // 删除页目录管理的所有页面及页目录本身，以上是依次遍历页目录的过程

    /* Hint: return the environment to the free list. */
    // 将进程加到free_list中，并在调度队列中删除进程
    // e->env_status = ENV_FREE;
    LIST_INSERT_HEAD(&env_free_list, e, env_link);
}

/* Overview:
 *  Free env e, and schedule to run a new env if e is the current env.
 *  释放当前进程，运行新进程
 */
void
env_destroy(struct Env *e)
{
    /* Hint: free e. */
    env_free(e);

    /* Hint: schedule to run a new environment. */
    if (curenv == e) {
        curenv = NULL;
        /* Hint: Why this? */
        bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),  // 从Kernel区域拷贝到TIMESTACK区域
              (void *)TIMESTACK - sizeof(struct Trapframe),
                sizeof(struct Trapframe));
        printf("i am env, i am killed ... \n");
        LIST_REMOVE(e, env_link);
        sched_yield();
    }
}

extern void env_pop_tf(struct Trapframe *tf, int id);  // id放到了CP0_ENTRYHI中作为TLB的index
extern void lcontext(u_int contxt);

// lab4-challenge
u_int mktcbid(struct Env *e, u_int thread_no) {
    return (e->env_id << 3) | thread_no;  // 线程号构造方法为env_id与线程序号的拼接
}

int threadid2tcb(u_int threadid, struct Tcb **ptcb) {
	struct Tcb *t;
	struct Env *e;

	if (threadid == 0) {  // 仿照envid2env，当threadid = 0时同样输出当前线程
		*ptcb = curtcb;
		return 0;
	}

	e = &envs[ENVX(threadid >> 3)];
	t = &e->env_threads[threadid & 0x7];
	if (t->tcb_status == ENV_FREE || t->tcb_id != threadid) {
		*ptcb = 0;
		return -E_THREAD_NOT_FOUND;
	}
	*ptcb = t;
	return 0;
}

int thread_alloc(struct Env *e, struct Tcb **thread) {
    if(e->env_thread_count >= MAX_THREAD){
        return -E_THREAD_MAX;
    }
    u_int thread_no = 0;
    while (thread_no < MAX_THREAD && e->env_threads[thread_no].tcb_status != THREAD_FREE) {
        thread_no++;
    }
    e->env_thread_count++;

    // 进行thread自身的初始化
    struct Tcb *t = &e->env_threads[thread_no];
    t->tcb_id = mktcbid(e, thread_no);
    t->env_id = e->env_id;
    t->tcb_status = THREAD_RUNNABLE;
    t->tcb_tf.cp0_status = 0x1000100c;
    t->tcb_tf.regs[29] = USTACKTOP - 4 * BY2PG * (t->tcb_id & 0x7);  // 设置线程的栈空间
    t->tcb_exit_ptr = (void *)0;
    t->tcb_detach_state = JOINABLE_STATE;
    t->join_times = 0;
    t->tcb_irq = PTHREAD_INTERRUPT_ON;
    LIST_INIT(&t->tcb_joined_list);
    *thread = t;
    printf("Thread alloc succeed! Thread id is 0x%x\n", t->tcb_id);
    return 0;
}

void thread_free(struct Tcb *t) {
    struct Env *e;
    envid2env(t->env_id, &e, 0);
    printf("[%08x] free tcb %08x\n", e->env_id, t->tcb_id);
	e->env_thread_count--;
	// if (e->env_thread_count == 0) {
	// 	env_free(e);
    //  printf("env thread count is %d\n", e->env_thread_count);
    // }
	t->tcb_status = THREAD_FREE;
}

void thread_destroy(struct Tcb *t) {
	if (t->tcb_status == ENV_RUNNABLE){
		LIST_REMOVE(t, tcb_sched_link);
        // printf("remove %x from sched_list\n", t->tcb_id);
    }

	thread_free(t);
	if (curtcb == t) {
		curtcb = NULL;
		bcopy((void *)KERNEL_SP - sizeof(struct Trapframe),
			(void *)TIMESTACK - sizeof(struct Trapframe),
			sizeof(struct Trapframe));
		printf("i am thread, i am killed ... \n");
        // 检查进程中的线程是否全被kill了
        struct Env *e;
        envid2env(t->env_id, &e, 0);
        // printf("remain threads is %d\n", e->env_thread_count);
        if(e->env_thread_count == 0){
            printf("all threads of env are killed, kill the env\n");
            env_destroy(e);
        } else {
            sched_yield();
        }
	}
}

void thread_run(struct Tcb *t) {
    if(curtcb) {  // 此时当前进程已经被时钟中断了，将上下文环境从TIMESTACK拷贝到TrapFrame中保存起来
        struct Trapframe *tf = (struct Trapframe *)(TIMESTACK - sizeof(struct Trapframe));
        bcopy((void *)tf, // source: TIMESTACK区域存储中断时的CPU寄存器
              (void *)(&(curtcb->tcb_tf)), sizeof(struct Trapframe));  // target: 当前进程的env_tf区域
        curtcb->tcb_tf.pc = tf->cp0_epc;  // 当前进程的pc设置成cp0_epc寄存器中的值
    }

    curtcb = t;
    envid2env(t->env_id, &curenv, 0);
    curenv->env_runs++;
    // printf("addr is %x\n", t);

    /* Step 3: Use lcontext() to switch to its address space. */
    lcontext(curenv->env_pgdir);  // 设置全局变量mCONTEXT为当前进程页目录地址，这个值将在TLB重填时用到。

    // 调用 env_pop_tf 函数，恢复现场(有从TrapFrame中取出寄存器的值到当前环境的操作)、异常返回。
    env_pop_tf(&(curtcb->tcb_tf), GET_ENV_ASID(curenv->env_id));
}