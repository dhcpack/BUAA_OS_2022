#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>
#include <sched.h>
#include <error.h>

extern char *KERNEL_SP;
extern struct Env *curenv;

/* Overview:
 * 	This function is used to print a character on screen.
 *
 * Pre-Condition:
 * 	`c` is the character you want to print.
 * 内核态中系统调用的实现
 */
void sys_putchar(int sysno, int c, int a2, int a3, int a4, int a5)
{
	printcharc((char) c);
	return ;
}

/* Overview:
 * 	This function enables you to copy content of `srcaddr` to `destaddr`.
 *
 * Pre-Condition:
 * 	`destaddr` and `srcaddr` can't be NULL. Also, the `srcaddr` area
 * 	shouldn't overlap the `destaddr`, otherwise the behavior of this
 * 	function is undefined.
 *
 * Post-Condition:
 * 	the content of `destaddr` area(from `destaddr` to `destaddr`+`len`) will
 * be same as that of `srcaddr` area.
 */
void *memcpy(void *destaddr, void const *srcaddr, u_int len)
{
	char *dest = destaddr;
	char const *src = srcaddr;

	while (len-- > 0) {
		*dest++ = *src++;
	}

	return destaddr;
}

/* Overview:
 *	This function provides the environment id of current process.
 *
 * Post-Condition:
 * 	return the current environment id
 */
u_int sys_getenvid(void)
{
	return curenv->env_id;
}

/* Overview:
 *	This function enables the current process to give up CPU.
 *
 * Post-Condition:
 * 	Deschedule current process. This function will never return.
 *
 * Note:
 *  For convenience, you can just give up the current time slice.
 * sched_yield
 *  放弃当前进程
 */
/*** exercise 4.6 ***/
void sys_yield(void)
{
	// 从Kernel系统中断区域拷贝到TIMESTACK时钟中断区域, 因为env_run默认保存在TIMESTACK区域
	bcopy((void *)KERNEL_SP - sizeof(struct Trapframe), (void *)TIMESTACK - sizeof(struct Trapframe), sizeof(struct Trapframe));
	sched_yield();  // 时钟调度其它进程
    return;
}

/* Overview:
 * 	This function is used to destroy the current environment.
 *
 * Pre-Condition:
 * 	The parameter `envid` must be the environment id of a
 * process, which is either a child of the caller of this function
 * or the caller itself.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 when error occurs.
 */
int sys_env_destroy(int sysno, u_int envid)
{
	/*
		printf("[%08x] exiting gracefully\n", curenv->env_id);
		env_destroy(curenv);
	*/
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0) {
		return r;
	}

	printf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

/* Overview:
 * 	Set envid's pagefault handler entry point and exception stack.
 *
 * Pre-Condition:
 * 	xstacktop points one byte past exception stack.
 *
 * Post-Condition:
 * 	The envid's pagefault handler will be set to `func` and its
 * 	exception stack will be set to `xstacktop`.
 * 	Returns 0 on success, < 0 on error.
 */
/*** exercise 4.12 ***/
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop)
{
	// Your code here.
	struct Env *env;
	int ret;
	if((ret = envid2env(envid, &env, 0)) != 0) {
		return ret;
	}

	env->env_pgfault_handler = func;
	env->env_xstacktop = xstacktop;

	return 0;
	//	panic("sys_set_pgfault_handler not implemented");
}

/* Overview:
 * 	Allocate a page of memory and map it at 'va' with permission
 * 'perm' in the address space of 'envid'.
 *
 * 	If a page is already mapped at 'va', that page is unmapped as a
 * side-effect.
 *
 * Pre-Condition:
 * perm -- PTE_V is required,
 *         PTE_COW is not allowed(return -E_INVAL),
 *         other bits are optional.
 *
 * Post-Condition:
 * Return 0 on success, < 0 on error
 *	- va must be < UTOP
 *	- env may modify its own address space or the address space of its children
 */
/*** exercise 4.3 ***/
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm)
{
	// Your code here.
	struct Env *env;
	struct Page *ppage;
	int ret;
	
	if(va >= UTOP) {
		return -E_INVAL;
	}
	if(((perm & PTE_COW) != 0) || ((perm & PTE_V) == 0)) {
		return -E_INVAL;
	}

	if((ret = envid2env(envid, &env, 1)) != 0) {  //TODO 这里的checkperm应该是1  env may modify its own address space or the address space of its children
		return ret;
	}
	if((ret = page_alloc(&ppage) != 0)) {
		return ret;
	}
	if((ret = page_insert(env->env_pgdir, ppage, va, perm)) != 0) {
		return ret;  // page_insert里面自带remove掉之前的页面
	}
	return 0;
}

/* Overview:
 * 	Map the page of memory at 'srcva' in srcid's address space
 * at 'dstva' in dstid's address space with permission 'perm'.
 * Perm must have PTE_V to be valid.
 * (Probably we should add a restriction that you can't go from
 * non-writable to writable?)
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Note:
 * 	Cannot access pages above UTOP.
 * // page_alloc，page_insert，page_lookup
 */
/*** exercise 4.4 ***/
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
				u_int perm)
{
	int ret;
	u_int round_srcva, round_dstva;
	struct Env *srcenv;
	struct Env *dstenv;
	struct Page *ppage;
	Pte *ppte;

	ppage = NULL;
	ret = 0;
	round_srcva = ROUNDDOWN(srcva, BY2PG);
	round_dstva = ROUNDDOWN(dstva, BY2PG);

    //your code here
	if(srcva >= UTOP || dstva >= UTOP) {
		return -E_INVAL;
	}
	if((perm & PTE_V) == 0) {  // not valid
		return -E_INVAL;
	}

	if((ret = envid2env(srcid, &srcenv, 0)) != 0) {  //TODO 这里的checkperm应该时0还是1 
		return ret;
	}
	if((ret = envid2env(dstid, &dstenv, 0)) != 0) {  //TODO 这里的checkperm应该时0还是1 
		return ret;
	}

	ppage = page_lookup(srcenv->env_pgdir, round_srcva, &ppte);
	if (ppage == NULL) {
		return -E_INVAL;
	}

	//you can't go from non-writable to writable
	if(((*ppte & PTE_R) == 0) && ((perm & PTE_R) != 0)) {  // 不能从不可写页面映射到可写页面
		return -E_INVAL;
	}

	if((ret = page_insert(dstenv->env_pgdir, ppage, round_dstva, perm)) != 0) {
		return ret;  // page_insert里面自带remove掉之前的页面
	}
	return 0;
}

/* Overview:
 * 	Unmap the page of memory at 'va' in the address space of 'envid'
 * (if no page is mapped, the function silently succeeds)
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Cannot unmap pages above UTOP.
 */
/*** exercise 4.5 ***/
int sys_mem_unmap(int sysno, u_int envid, u_int va)
{
	// Your code here.
	int ret;
	struct Env *env;

	if(va >= UTOP) {
		return -E_INVAL;
	}

	if((ret = envid2env(envid, &env, 0)) != 0) {  //TODO 这里的checkperm应该时0还是1 
		return ret;
	}

	page_remove(env->env_pgdir, va);

	return 0;
	//	panic("sys_mem_unmap not implemented");
}

/* Overview:
 * 	Allocate a new environment.
 *
 * Pre-Condition:
 * The new child is left as env_alloc created it, except that
 * status is set to ENV_NOT_RUNNABLE and the register set is copied
 * from the current environment.
 *
 * Post-Condition:
 * 	In the child, the register set is tweaked so sys_env_alloc returns 0.
 * 	Returns envid of new environment, or < 0 on error.
 */
/*** exercise 4.8 ***/
int sys_env_alloc(void)
{
	// Your code here.
	int r;
	struct Env *e;
	if((r = env_alloc(&e, curenv->env_id)) != 0) {  // 新建的进程页表和页目录是不同的
		return r;
	}
	// 复制运行现场
	// bcopy((void *)(KERNEL_SP - sizeof(struct Trapframe)), (void *)&(e->env_tf), sizeof(struct Trapframe));
	bcopy((void *)(KERNEL_SP - sizeof(struct Trapframe)), (void*)&(e->env_threads[0].tcb_tf), sizeof(struct Trapframe));
	// 设置子进程的程序计数器  cp0_epc中的值已经加过4了 即syscall的后一条指令 相当于子进程的entry_point
	e->env_threads[0].tcb_tf.pc = e->env_threads[0].tcb_tf.cp0_epc;
	// e->env_tf.pc = e->env_tf.cp0_epc;

	// 更改子进程返回值 $v0 = 0
	e->env_threads[0].tcb_tf.regs[2] = 0;
	// e->env_tf.regs[2] = 0;  // 子进程的返回值被赋值为0，在进程被调度时返回

	// 其它初始化
	e->env_threads[0].tcb_pri = curenv->env_threads[0].tcb_pri;
	e->env_threads[0].tcb_status = ENV_NOT_RUNNABLE;
	// e->env_status = ENV_NOT_RUNNABLE;  // except that status is set to ENV_NOT_RUNNABLE

	return e->env_id;   // 父进程的返回值是新建的子进程的envid
	//	panic("sys_env_alloc not implemented");
}

/* Overview:
 * 	Set envid's env_status to status.
 *
 * Pre-Condition:
 * 	status should be one of `ENV_RUNNABLE`, `ENV_NOT_RUNNABLE` and
 * `ENV_FREE`. Otherwise return -E_INVAL.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if status is not a valid status for an environment.
 * 	The status of environment will be set to `status` on success.
 */
/*** exercise 4.14 ***/
int sys_set_env_status(int sysno, u_int envid, u_int status)
{
	// Your code here.
	struct Env *env;
	struct Tcb *t;
	int ret;

	if(status != ENV_FREE && status != ENV_NOT_RUNNABLE && status != ENV_RUNNABLE){
		return -E_INVAL;
	}

	if((ret = envid2env(envid, &env, 0)) != 0){
		return ret;
	}

	// t = &env->env_threads[0];
	
	// if(status == ENV_RUNNABLE && t->tcb_status != ENV_RUNNABLE) {
	// 	LIST_INSERT_HEAD(tcb_sched_list, t, tcb_sched_link);
	// } else if(status != ENV_RUNNABLE && t->tcb_status == ENV_RUNNABLE) {
	// 	LIST_REMOVE(t, tcb_sched_link);
	// }
	// env->env_threads[0].tcb_status = status;
	// // printf("set succeed\n");
	// if(status == ENV_FREE) {
	// 	env_destroy(env);  // TODO
	// }
	// return 0;
	// //	panic("sys_env_set_status not implemented");
	int thread_no = 0;
	for (thread_no = 0; thread_no < MAX_THREAD; thread_no++){
		t = &env->env_threads[thread_no];
		if(t->tcb_status == THREAD_FREE){
			continue;
		}
		if(status == ENV_RUNNABLE && t->tcb_status == THREAD_NOT_RUNNABLE) {
			LIST_INSERT_HEAD(tcb_sched_list, t, tcb_sched_link);
			t->tcb_status = status;
		} else if(status != ENV_RUNNABLE && t->tcb_status == ENV_RUNNABLE) {
			LIST_REMOVE(t, tcb_sched_link);
			t->tcb_status = status;
		}
	}
	if(status == ENV_FREE) {
		env_destroy(env);  // TODO
	}
	return 0;
}

/* Overview:
 * 	Set envid's trap frame to tf.
 *
 * Pre-Condition:
 * 	`tf` should be valid.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if the environment cannot be manipulated.
 *
 * Note: This hasn't be used now?
 */
int sys_set_trapframe(int sysno, u_int envid, struct Trapframe *tf)
{

	return 0;
}

/* Overview:
 * 	Kernel panic with message `msg`.
 *
 * Pre-Condition:
 * 	msg can't be NULL
 *
 * Post-Condition:
 * 	This function will make the whole system stop.
 */
void sys_panic(int sysno, char *msg)
{
	// no page_fault_mode -- we are trying to panic!
	panic("%s", TRUP(msg));
}

/* Overview:
 * 	This function enables caller to receive message from
 * other process. To be more specific, it will flag
 * the current process so that other process could send
 * message to it.
 *
 * Pre-Condition:
 * 	`dstva` is valid (Note: NULL is also a valid value for `dstva`).
 *
 * Post-Condition:
 * 	This syscall will set the current process's status to
 * ENV_NOT_RUNNABLE, giving up cpu.
 */
/*** exercise 4.7 ***/
void sys_ipc_recv(int sysno, u_int dstva)
{
	if(dstva >= UTOP) {
		return;
	}
	// 首先要将自身的env_ipc_recving设置为1，表明该进程准备接受发送方的消息
	// 之后给env_ipc_dstva赋值，表明自己要将接受到的页面与dstva完成映射
	// 阻塞当前进程，即把当前进程的状态置为不可运行（ENV_NOT_RUNNABLE）
	// 最后放弃CPU（调用相关函数重新进行调度），安心等待发送方将数据发送过来
	curenv->env_ipc_recving = 1;
	curenv->env_ipc_waiting_thread_no = curtcb->tcb_id & 0x7;
	curenv->env_ipc_dstva = dstva;
	if (curtcb->tcb_status == ENV_RUNNABLE){
		LIST_REMOVE(curtcb,tcb_sched_link);
	}
	curtcb->tcb_status = ENV_NOT_RUNNABLE;
	sys_yield();
}

/* Overview:
 * 	Try to send 'value' to the target env 'envid'.
 *
 * 	The send fails with a return value of -E_IPC_NOT_RECV if the
 * target has not requested IPC with sys_ipc_recv.
 * 	Otherwise, the send succeeds, and the target's ipc fields are
 * updated as follows:
 *    env_ipc_recving is set to 0 to block future sends
 *    env_ipc_from is set to the sending envid
 *    env_ipc_value is set to the 'value' parameter
 * 	The target environment is marked runnable again.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Hint: the only function you need to call is envid2env.
 */
/*** exercise 4.7 ***/
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
					 u_int perm)
{
	int r;
	struct Env  *e;
	struct Page *p;
	struct Tcb  *t;
	// 根据envid找到相应进程，如果指定进程为可接收状态(考虑env_ipc_recving)，则发送成功
	// 否则，函数返回-E_IPC_NOT_RECV，表示目标进程未处于接受状态
	// 清除接收进程的接收状态，将相应数据填入进程控制块，传递物理页面的映射关系
	// 修改进程控制块中的进程状态，使接受数据的进程可继续运行(ENV_RUNNABLE)
	if(srcva >= UTOP) {
		return -E_INVAL;
	}
	if((r = envid2env(envid, &e, 0)) != 0) {
		return r;
	}
	if(e->env_ipc_recving == 0) {
		return -E_IPC_NOT_RECV;
	}
    if(e->env_ipc_dstva >= UTOP) {
        return -E_INVAL;
    }

	t = &e->env_threads[e->env_ipc_waiting_thread_no];
    if(srcva != 0) {
		p = page_lookup(curenv->env_pgdir, srcva, NULL);
		if(p == NULL) {
			return -E_INVAL;
		}
		if((r = page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm)) != 0){
			return r;
		}
	}

	e->env_ipc_from = curenv->env_id;
	e->env_ipc_value = value;
	e->env_ipc_perm = perm;
	e->env_ipc_recving = 0;
	t->tcb_status = ENV_RUNNABLE;
	LIST_INSERT_HEAD(tcb_sched_list, t, tcb_sched_link);
	return 0;
}

// lab4-challenge
u_int sys_get_threadid(void)
{
	return curtcb->tcb_id;
}

int sys_thread_destroy(int sysno, u_int threadid)
{
	int r;
	struct Tcb *t;
	if ((r = threadid2tcb(threadid, &t)) < 0) {
		return r;
	}
	struct Tcb *tmp;
	while (!LIST_EMPTY(&t->tcb_joined_list)) {  // 该进程结束，所有在join队列中的进程开始运行
		tmp = LIST_FIRST(&t->tcb_joined_list);
		LIST_REMOVE(tmp, tcb_joined_link);
		*(tmp->tcb_join_retval) = t->tcb_exit_ptr;  // 指针指向接收到的返回值
		// printf("wake up tcbid is 0x%x\n",tmp->thread_id);
		tmp->join_times--;
		// printf("tcb%x join times is %d\n",tmp->tcb_id, tmp->join_times);
		if(tmp->join_times == 0){
			sys_set_thread_status(0, tmp->tcb_id, THREAD_RUNNABLE);
			printf("tcb %08x in joinlist wake up\n", tmp->tcb_id);
		}
	}
	printf("[%08x] destroying tcb %08x\n", curenv->env_id, t->tcb_id);
	thread_destroy(t);
	return 0;
}

int sys_thread_alloc(void)
{
	int r;
	struct Tcb *t;
	if(!curenv){
		return -E_BAD_ENV;
	}
	if((r = thread_alloc(curenv, &t)) != 0){
		return r;
	}
	// 进行和ENV相关的初始化
	t->tcb_pri = curenv->env_threads[0].tcb_pri;
	t->tcb_status = THREAD_NOT_RUNNABLE;
	t->tcb_tf.regs[2] = 0;  // 返回值设置为0
	t->tcb_tf.pc = t->tcb_tf.cp0_epc;
	return t->tcb_id & 0x7;
}

int sys_set_thread_status(int sysno, u_int threadid, u_int status){
	struct Tcb *t;
	int r;

	if(status != THREAD_FREE && status != THREAD_NOT_RUNNABLE && status != THREAD_RUNNABLE){
		return -E_INVAL;
	}
	if((r = threadid2tcb(threadid, &t) != 0)){
		return r;
	}

	if(status == THREAD_RUNNABLE && t->tcb_status != THREAD_RUNNABLE) {
		LIST_INSERT_HEAD(tcb_sched_list, t, tcb_sched_link);
		// printf("tcb 0x%x insert succeed\n", t->tcb_id);
	} else if(status != THREAD_RUNNABLE && t->tcb_status == THREAD_RUNNABLE) {
		LIST_REMOVE(t, tcb_sched_link);
	}
	t->tcb_status = status;
	return 0;
}
