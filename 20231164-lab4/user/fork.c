// implement fork from user space

#include "lib.h"
#include <mmu.h>
#include <env.h>


/* ----------------- help functions ---------------- */

/* Overview:
 * 	Copy `len` bytes from `src` to `dst`.
 *
 * Pre-Condition:
 * 	`src` and `dst` can't be NULL. Also, the `src` area
 * 	 shouldn't overlap the `dest`, otherwise the behavior of this
 * 	 function is undefined.
 */
void user_bcopy(const void *src, void *dst, size_t len)
{
    void *max;

    //	writef("~~~~~~~~~~~~~~~~ src:%x dst:%x len:%x\n",(int)src,(int)dst,len);
    max = dst + len;

    // copy machine words while possible
    if (((int)src % 4 == 0) && ((int)dst % 4 == 0)) {
        while (dst + 3 < max) {
            *(int *)dst = *(int *)src;
            dst += 4;
            src += 4;
        }
    }

    // finish remaining 0-3 bytes
    while (dst < max) {
        *(char *)dst = *(char *)src;
        dst += 1;
        src += 1;
    }

    //for(;;);
}

/* Overview:
 * 	Sets the first n bytes of the block of memory
 * pointed by `v` to zero.
 *
 * Pre-Condition:
 * 	`v` must be valid.
 *
 * Post-Condition:
 * 	the content of the space(from `v` to `v`+ n)
 * will be set to zero.
 */
void user_bzero(void *v, u_int n)
{
    char *p;
    int m;

    p = v;
    m = n;

    while (--m >= 0) {
        *p++ = 0;
    }
}
/*--------------------------------------------------------------*/

/* Overview:
 * 	Custom page fault handler - if faulting page is copy-on-write,
 * map in our own private writable copy.
 *
 * Pre-Condition:
 * 	`va` is the address which leads to a TLBS exception.
 *
 * Post-Condition:
 *  Launch a user_panic if `va` is not a copy-on-write page.
 * Otherwise, this handler should map a private writable copy of
 * the faulting page at correct address.
 */
/*** exercise 4.13 ***/
static void
pgfault(u_int va)
{
    u_int *tmp;
    tmp = USTACKTOP;   // 页面的地址还是用指针类型存^-^
    u_int perm = ((Pte *)(*vpt))[va >> PGSHIFT] & 0xfff;
    //	writef("fork.c:pgfault():\t va:%x\n",va);
    if((perm & PTE_COW) == 0) {
        user_panic("faulting page is not copy-on-write");
    }
    perm -= PTE_COW;
    va = ROUNDDOWN(va, BY2PG);

    //map the new page at a temporary place  在没用的虚拟地址处临时申请一个临时物理页面(利用这个虚拟地址进行复制？)
    syscall_mem_alloc(0, tmp, perm);

    //copy the content va必须字对齐
    user_bcopy(va, tmp, BY2PG);   // 复制页面内容(用虚拟地址索引，进行物理页面意义上的复制)

    //map the page on the appropriate place  将复制好的页面绑定到恰当位置(tmp对应的物理页面绑定到虚拟地址va处)
    syscall_mem_map(0, tmp, 0, va, perm);

    //unmap the temporary placev 将临时页面解绑(将tmp对应的物理页面和tmp解绑)
    syscall_mem_unmap(0, tmp);
}

/* Overview:
 * 	Map our virtual page `pn` (address pn*BY2PG) into the target `envid`
 * at the same virtual address.
 *
 * Post-Condition:
 *  if the page is writable or copy-on-write, the new mapping must be
 * created copy on write and then our mapping must be marked
 * copy on write as well. In another word, both of the new mapping and
 * our mapping should be copy-on-write if the page is writable or
 * copy-on-write.
 *
 * Hint:
 * 	PTE_LIBRARY indicates that the page is shared between processes.
 * A page with PTE_LIBRARY may have PTE_R at the same time. You
 * should process it correctly.
 * vpt: // 自映射页表向虚拟地址
	.word UVPT
	vpd: // 自映射页目录虚拟地址
	.word (UVPT+(UVPT>>12)*4)
	extern volatile Pte* vpt[];
	extern volatile Pde* vpd[];
 */
/*** exercise 4.10 ***/
static void
duppage(u_int envid, u_int pn)
{
    u_int addr;
    u_int perm;

    addr = pn << PGSHIFT;  // virtual address = pn * BY2PG   pn: virtual page
    perm = ((Pte *)(*vpt))[pn] & 0xfff;  // *vpt是页表项虚拟地址，((Pte *)(*vpt))是一个指向第一个页表项的指针，((Pte *)(*vpt))[pn]是第pn个页表项
	int flag = 0;
	if (((perm & PTE_R) != 0) && ((perm & PTE_LIBRARY) == 0)) {   // 将是可写页面，且不是共享页面加上写时复制标志位
		perm = perm | PTE_COW;
		flag = 1;
	}

	syscall_mem_map(0, addr, envid, addr, perm);

    // 如果父进程在两次映射间调入了被映射的页面就会让父进程的这一页丢掉 PTE_R
    // 这样之后一次映射就属于 from non-writable to writable
    // 如果父进程不仅调入而且写入了这一页就会直接让这一页对应的物理页改变
    // 这样之后映射的物理页就错了，所以要先给子进程映射
	if(flag == 1) {
		syscall_mem_map(0, addr, 0, addr, perm); // 如果改变了标志位，需要同时改变当前进程，也就是父进程(0 -> current_env)
	}
    //	user_panic("duppage not implemented");
    // 先映射父进程的时候如果父进程写入映射页触发了缺页异常，就会导致原有的PTE_R位消失
}

/* Overview:
 * 	User-level fork. Create a child and then copy our address space
 * and page fault handler setup to the child.
 *
 * Hint: use vpd, vpt, and duppage.
 * Hint: remember to fix "env" in the child process!
 * Note: `set_pgfault_handler`(user/pgfault.c) is different from
 *       `syscall_set_pgfault_handler`.
 */
/*** exercise 4.9 4.15***/
extern void __asm_pgfault_handler(void);
int
fork(void)
{
    // Your code here.
    u_int newenvid;
    extern struct Env *envs;
    extern struct Env *env;
    u_int i;

    // The parent installs pgfault using set_pgfault_handler                                                                                                    
    set_pgfault_handler(pgfault);  // 父进程设置异常处理函数
    // alloc a new alloc
    newenvid = syscall_env_alloc();

    // writef("newenvid get: newenvid = %d\n", newenvid);
    if(newenvid == 0) { // 子进程
        // envid2env(syscall_getenvid(), &env, 0);  // ?? 这里因为不知名原因找不到envid2env函数
        env = envs + ENVX(syscall_getenvid());
        return 0;
    }

    //	writef("dump page begin\n");
    // 遍历父进程地址空间，进行duppage。
    for (i = 0; i < VPN(USTACKTOP); i++) {
        if((((Pde *)(*vpd))[i>>10] & PTE_V) && (((Pte *)(*vpt))[i] & PTE_V)){  // 如果一级页表项和二级页表项都Valid，则duppage
            duppage(newenvid, i);
        }
    }
    //   writef("duppage finish\n");

    // 为子进程分配异常处理栈。
    syscall_mem_alloc(newenvid, UXSTACKTOP - BY2PG, PTE_V | PTE_R);  // 子进程分配异常栈空间  // 注意这里代入的是UXSTACKTOP - BY2PG而不是UXSTACKTOP

    // 设置子进程的异常处理函数，确保页写入异常可以被正常处理。    
    syscall_set_pgfault_handler(newenvid, __asm_pgfault_handler, UXSTACKTOP);  // 子进程设置异常处理函数

    // 设置子进程的运行状态
    syscall_set_env_status(newenvid, ENV_RUNNABLE);

    return newenvid;
}

// Challenge!
int
sfork(void)
{
    user_panic("sfork not implemented");
    return -E_INVAL;
}
