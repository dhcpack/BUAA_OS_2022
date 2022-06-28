#include "lib.h"
#include <error.h>
#include <mmu.h>

/* 
 * parameter meanings:
 * pthread_t *thread: 线程的标识，系统调用的返回值
 * const pthread_attr_t *attr: 手动设置线程的属性
 * void *(*start_routine) (void *): 以函数指针的方式指明新建线程需要执行的函数
 * void *arg: 指定传递给 start_routine 函数的实参
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg){
    int r;
    if((r = syscall_thread_alloc()) < 0){
        return r;
    }
    u_int thread_no = r;
    struct Tcb *t = &env->env_threads[thread_no];

    t->tcb_tf.regs[29] = USTACKTOP - 4 * BY2PG * thread_no;  // 设置栈指针
	t->tcb_tf.pc = start_routine;  // 设置起始PC
	t->tcb_tf.regs[29] -= 4;  // ?
	t->tcb_tf.regs[4] = arg;  // 传递给函数的参数
	t->tcb_tf.regs[31] = exit;  // 线程的返回地址
	t->tcb_detach_state = attr->detach_state;  // attr仅用来表示分离状态

	syscall_set_thread_status(t->tcb_id, THREAD_RUNNABLE);
	*thread = t->tcb_id;
	return 0;
}

/* 
 * parameter meanings:
 * retval: 一个指向返回状态值的指针
 */
void pthread_exit(void *retval){
    u_int threadid = syscall_get_threadid();
    struct Tcb *t = &env->env_threads[threadid & 0x7];
	t->tcb_exit_ptr = retval;
	syscall_thread_destroy(threadid);
}

/* 
 * usage:
 * 多线程程序中，一个线程可以借助pthread_cancel()函数向另一个线程发送信号，终止另一个线程的执行。
 * 
 * parameter meanings:
 * thread: 目标线程(接收终止信号的线程)
 */
int pthread_cancel(pthread_t thread){
    int r;
    struct Tcb *t = &env->env_threads[thread&0x7];
	if ((t->tcb_id != thread) || (t->tcb_status == THREAD_FREE)) {
		return -E_THREAD_NOT_FOUND;
	}
	// if (t->tcb_cancelstate == THREAD_CANNOT_BE_CANCELED) {
	// 	return -E_THREAD_CANNOTCANCEL;
	// }
	// t->tcb_exit_value = -THREAD_CANCELED_EXIT;
	// if (t->tcb_canceltype == THREAD_CANCEL_IMI) {
	// 	syscall_thread_destroy(thread);
	// } else {
	// 	t->tcb_canceled = 1;
	// }
    if((r = syscall_thread_destroy(thread)) != 0){
        return r;
    }
    return 0;
}

/* 
 * parameter meanings:
 * thread: 用于指定接收哪个线程的返回值；
 * retval: 表示接收到的返回值
 * 
 * requirements:
 * 目标进程必须是joinable
 */
int pthread_join(pthread_t thread, void **retval){
    return syscall_thread_join(thread, retval);
}

/* 
 * parameter meanings:
 * thread: 分离的线程
 * 
 * results:
 * 线程分离后资源自动回收，不能被join
 */
int pthread_detach(pthread_t thread) {
	struct Tcb *t = &env->env_threads[thread & 0x7];
	int r;
	if (t->tcb_id != thread) {
		return -E_THREAD_NOT_FOUND;
	}
	if (t->tcb_status == THREAD_FREE) {
		u_int sp = USTACKTOP - BY2PG * 4 * (thread & 0x7);  // 当前线程栈顶
		int i;
		for(i = 1; i <= 4; ++i) {
			if((r = syscall_mem_unmap(0, sp - i * BY2PG) != 0)) {  // 把线程占有的空间释放
				return r;
			}
		}
		user_bzero(t, sizeof(struct Tcb));  // 释放线程所有资源
	} else if (t->join_times != 0){
		return -E_THREAD_DETACHED_FAIL;
	} else {
		t->tcb_detach_state = DETACHED_STATE;
	}
	return 0;
}

/* 
 * parameter meanings:
 * pthread_attr_t * attr: 指向线程属性结构体的指针
 * detachstate: 分离状态 PTHREAD_CREATE_JOINABLE or PTHREAD_CREATE_DETACHED
 * 
 * results:
 * 线程分离后资源自动回收，不能被join
 */
int pthread_attr_setdetachstate(pthread_attr_t * attr, int detachstate) {
	attr->detach_state = detachstate;
}

/* 
 * results:
 * 得到当前线程id
 */
pthread_t pthread_self(void) {
	return syscall_get_threadid();
}