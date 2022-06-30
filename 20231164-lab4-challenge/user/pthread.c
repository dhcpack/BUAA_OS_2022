#include "lib.h"
#include <error.h>
#include <mmu.h>

/* 
 * parameter meanings:
 * 	pthread_t *thread: 线程的标识，系统调用的返回值
 * 	const pthread_attr_t *attr: 手动设置线程的属性
 * 	void *(*start_routine) (void *): 以函数指针的方式指明新建线程需要执行的函数
 * 	void *arg: 指定传递给 start_routine 函数的实参
 * 
 * results:
 * 	创建一个进程
 * 
 * errors:
 * 	当前进程异常: -E_BAD_ENV
 *  进程中的线程已经达到最大数量: -E_THREAD_MAX
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
	// t->tcb_tf.regs[29] -= 4;  // ?
	t->tcb_tf.regs[4] = arg;  // 传递给函数的参数
	t->tcb_tf.regs[31] = exit;  // 线程的返回地址
	if(attr == 0) {
		t->tcb_detach_state = JOINABLE_STATE;
	} else {
		t->tcb_detach_state = attr->detach_state;  // attr仅用来表示分离状态
	}
	

	syscall_set_thread_status(t->tcb_id, THREAD_RUNNABLE);
	*thread = t->tcb_id;
	return 0;
}

/* 
 * parameter meanings:
 *	retval: 一个指向返回状态值的指针
 * 
 * results:
 * 	线程退出，返回状态是retval
 * 
 * errors:
 * 	未找到线程: -E_THREAD_NOT_FOUND
 */
void pthread_exit(void *retval){
    u_int threadid = syscall_get_threadid();
    struct Tcb *t = &env->env_threads[threadid & 0x7];
	t->tcb_exit_ptr = retval;
	syscall_thread_destroy(threadid);
}

/* 
 * parameter meanings:
 *  thread: 用于指定接收哪个线程的返回值；
 *  retval: 表示接收到的返回值
 * 
 * requirements:
 *  目标进程必须是joinable
 * 
 * results:
 * 	当前线程阻塞，等待thread线程运行完成
 * 
 * errors:
 *  未找到线程: -E_THREAD_NOT_FOUND
 *  线程处于分离状态: -E_THREAD_JOIN_FAIL
 */
int pthread_join(pthread_t thread, void **retval){
    int r;
    struct Tcb *t = &env->env_threads[thread & 0x7];
	if (t->tcb_id != thread) {
		return -E_THREAD_NOT_FOUND;
	}
	if(t->tcb_detach_state != JOINABLE_STATE){
		writef("Target thread is not joinable, join fained.\n");
		return -E_THREAD_JOIN_FAIL;
	}
	if (t->tcb_status == THREAD_FREE) {  // 如果等待的线程已经结束，直接将retval指向线程的返回值
		if (retval != 0) {
			*retval = t->tcb_exit_ptr;
		}
		return 0;
	}

	struct Tcb *curtcb = &env->env_threads[pthread_self() & 0x7];
	LIST_INSERT_HEAD(&t->tcb_joined_list, curtcb, tcb_joined_link);  // 当前线程加到t的join队列中
	curtcb->join_times++;  // 当前线程的join次数++
	curtcb->tcb_join_retval = retval;  // 保存该地址
	if((r = syscall_set_thread_status(0, THREAD_NOT_RUNNABLE)) != 0) {  // 阻塞当前线程
		return r;
	}
	// struct Trapframe *trap = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe));
	// trap->regs[2] = 0;
	// trap->pc = trap->cp0_epc;
	syscall_yield();
	return 0;
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
	return 0;
}

/* 
 * results:
 * 得到当前线程id
 */
pthread_t pthread_self(void) {
	return syscall_get_threadid();
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
	int r;
	if(r = sem_init(&mutex->sem, 0, 1) != 0){
		return r;
	}
	mutex->reference_times = 0;
	mutex->envid = env->env_id;
	if(attr == NULL) {
		mutex->mutex_type = PTHREAD_MUTEX_ERRORCHECK;
		mutex->pshared = MUTEX_PROCESS_PRIVATE;
	} else {
		mutex->mutex_type = attr->mutex_type;
		mutex->pshared = attr->pshared;
	}
	mutex->status = MUTEX_UNLOCKING;
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
	if(mutex->envid != env->env_id && mutex->pshared != MUTEX_PROCESS_SHARED){
		return -E_MUTEX_NOT_FOUND;
	}
	if(mutex->status == MUTEX_LOCKING){
		return -E_MUTEX_USING;
	}
	if (mutex->status == MUTEX_FREE) {
		return 0;
	}
	mutex->status = MUTEX_FREE;
	return sem_destroy(&mutex->sem);
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
	if(mutex->envid != env->env_id && mutex->pshared != MUTEX_PROCESS_SHARED){
		return -E_MUTEX_NOT_FOUND;
	}
	if(mutex->status == MUTEX_FREE){
		return -E_MUTEX_NOT_FOUND;
	}
	if(mutex->status == MUTEX_UNLOCKING) {
		mutex->reference_times++;
		mutex->mutex_owner = pthread_self();
		mutex->status = MUTEX_LOCKING;
		return sem_wait(&mutex->sem);
	}
	if(mutex->mutex_type == PTHREAD_MUTEX_RECURSIVE && mutex->mutex_owner == pthread_self()) {
		mutex->reference_times++;
		return 0;
	}
	if(mutex->mutex_type == PTHREAD_MUTEX_ERRORCHECK && mutex->mutex_owner == pthread_self()){
		return -E_MUTEX_DEADLOCK;
	}
	return sem_wait(&mutex->sem);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
	if(mutex->envid != env->env_id && mutex->pshared != MUTEX_PROCESS_SHARED){
		return -E_MUTEX_NOT_FOUND;
	}
	if(mutex->status == MUTEX_FREE){
		return -E_MUTEX_NOT_FOUND;
	}
	if(mutex->status == MUTEX_UNLOCKING) {
		mutex->reference_times++;
		mutex->mutex_owner = pthread_self();
		mutex->status = MUTEX_LOCKING;
		return 0;
	}
	if(mutex->mutex_type == PTHREAD_MUTEX_RECURSIVE && mutex->mutex_owner == pthread_self()) {
		mutex->reference_times++;
		return 0;
	}
	return -E_MUTEX_AGAIN;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	if(mutex->envid != env->env_id && mutex->pshared != MUTEX_PROCESS_SHARED){
		return -E_MUTEX_NOT_FOUND;
	}
	if(mutex->status == MUTEX_FREE){
		return -E_MUTEX_NOT_FOUND;
	}
	if(mutex->status == MUTEX_UNLOCKING || mutex->mutex_owner != pthread_self()){
		return -E_MUTEX_UNLOCK_FAIL;
	}
	mutex->reference_times--;
	if(mutex->reference_times == 0) {
		if(mutex->sem.sem_wait_count == 0) {
			mutex->status = MUTEX_UNLOCKING;
		} else {
			mutex->reference_times++;
			mutex->mutex_owner = mutex->sem.sem_wait_list[mutex->sem.sem_tail_index]->tcb_id;
		}
		return sem_post(&mutex->sem);
	}
	return 0;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr,int pshared) {
	if(attr == NULL){
		return -E_INVAL;
	}
	if(pshared != MUTEX_PROCESS_PRIVATE && pshared != MUTEX_PROCESS_SHARED)
	attr->pshared = pshared;
	return 0;
}
int pthread_mutexattr_getpshared(pthread_mutexattr_t *attr,int *pshared) {
	if(attr == NULL || pshared == NULL){
		return -E_INVAL;
	}
	*pshared = attr->pshared;
	return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr , int type) {
	if(attr == NULL){
		return -E_INVAL;
	}
	if(type != PTHREAD_MUTEX_ERRORCHECK && type != PTHREAD_MUTEX_RECURSIVE){
		return -E_INVAL;
	}
	attr->mutex_type = type;
	return 0;
} 

int pthread_mutexattr_gettype(pthread_mutexattr_t *attr , int *type) {
	if(attr == NULL || type == NULL){
		return -E_INVAL;
	}
	*type = attr->mutex_type;
	return 0;
}

/* 
 * parameter meanings:
 *	thread: 目标线程(接收终止信号的线程)
 * 
 * results:
 *  删除thread线程
 * 
 * errors:
 *  未找到线程: -E_THREAD_NOT_FOUND
 */
// If a thread has disabled cancellation,
// then a cancellation request remains queued until the thread enables cancellation. 
// If a thread has enabled cancellation,
// then its cancelability type determines when cancellation occurs.
int pthread_cancel(pthread_t thread){
	int retval = PTHREAD_CANCELED;
	struct Tcb *t = &env->env_threads[thread & 0x7];
	if(t->tcb_id != thread || t->tcb_status == THREAD_FREE) {
		return -E_THREAD_NOT_FOUND;
	}
	if (t->tcb_cancelstate == PTHREAD_CANCEL_DISABLE || t->tcb_canceltype ==  PTHREAD_CANCEL_DEFERRED) {
		t->tcb_canceled = PTHREAD_CANCELED;
		t->tcb_exit_ptr = &retval;
		return 0;
	}
	
	t->tcb_exit_ptr = &retval;
	return syscall_thread_destroy(thread);
}

int  pthread_setcancelstate(int state, int *oldstate) {
	if(state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
		return -E_INVAL;
	}
	u_int threadid = pthread_self();
	struct Tcb *t = &env->env_threads[threadid & 0x7];
	if(t->tcb_id != threadid || t->tcb_status == THREAD_FREE) {
		return -E_THREAD_NOT_FOUND;
	}
	if(oldstate != NULL) {
		*oldstate = t->tcb_cancelstate;
	}
	t->tcb_cancelstate = state;
	if(t->tcb_cancelstate == PTHREAD_CANCEL_ENABLE && t->tcb_canceled == PTHREAD_CANCELED && t->tcb_canceltype == PTHREAD_CANCEL_ASYNCHRONOUS) {
		return syscall_thread_destroy(threadid);
	}
	return 0;
}

int  pthread_setcanceltype(int type, int *oldtype) {
	if(type != PTHREAD_CANCEL_DEFERRED && type != PTHREAD_CANCEL_ASYNCHRONOUS) {
		return -E_INVAL;
	}
	u_int threadid = pthread_self();
	struct Tcb *t = &env->env_threads[threadid & 0x7];
	if(t->tcb_id != threadid || t->tcb_status == THREAD_FREE) {
		return -E_THREAD_NOT_FOUND;
	}
	if(oldtype != NULL) {
		*oldtype = t->tcb_canceltype;
	}
	t->tcb_canceltype = type;
	return 0;
}

void pthread_testcancel(void) {
	u_int threadid = pthread_self();
	struct Tcb *t = &env->env_threads[threadid & 0x7];
	if(t->tcb_id != threadid || t->tcb_status == THREAD_FREE) {
		return;
	}
	if(t->tcb_cancelstate == PTHREAD_CANCEL_DISABLE) {
		return;
	}
	if(t->tcb_canceled == PTHREAD_CANCELED) {
		syscall_thread_destroy(threadid);
	}
}