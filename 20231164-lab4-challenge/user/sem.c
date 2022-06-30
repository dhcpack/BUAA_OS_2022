#include "lib.h"
#include <error.h>
#include <env.h>

/* 
 * parameter meanings:
 * sem_t *sem: 信号量
 * int pshared: pshared不为0时此信号量在进程间共享，否则只能为当前进程的所有线程共享
 * unsigned int value: 信号量初始值
 */
int sem_init(sem_t *sem, int pshared, unsigned int value) {
    if(sem == 0){
        return -E_INVAL;
    }
    sem->sem_envid = env->env_id;
    sem->sem_value = value;
    sem->sem_shared = pshared;
    sem->sem_status = SEM_VALID;
    sem->sem_wait_count = 0;
    sem->sem_head_index = 0;
    sem->sem_tail_index = 0;
    return 0;
}

/* 
 * parameter meanings:
 * sem_t *sem: 要删除的信号量
 */
int sem_destroy(sem_t *sem) {
    if((sem->sem_envid != env->env_id) && (sem->sem_shared == SEM_PROCESS_PRIVATE)){
        return -E_SEM_NOT_FOUND;  // 如果sem不是当前进程创建的，而且不是进程间公用的，返回-E_SEM_NOT_FOUND
    }
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_DISABLED);
    if(sem->sem_wait_count != 0) {  // 如果还有进程在等待当前信号量，返回-E_SEM_STILL_USED
        return -E_SEM_STILL_USED;
    }
    sem->sem_status = SEM_FREE;  // normal behavior 将sem标记为SEM_FREE
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
    return 0;
}

/* 
 * results:
 * P操作，若信号量小于等于0则阻塞线程
 */
int sem_wait(sem_t *sem){
    if((sem->sem_envid != env->env_id) && (sem->sem_shared == SEM_PROCESS_PRIVATE)){
        return -E_SEM_NOT_FOUND;  // 如果sem不是当前进程创建的，而且不是进程间公用的，返回-E_SEM_NOT_FOUND
    }
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_DISABLED);
    if(sem -> sem_status == SEM_FREE){
        syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
        return -E_SEM_FREE;
    }
    if(sem->sem_value > 0){
        sem->sem_value--;
        syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
        return 0;
    }
    if(sem->sem_wait_count >= MAX_WAIT_THREAD) {
        syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
        return -E_THREAD_MAX;
    }
    struct Tcb* t = &env->env_threads[syscall_get_threadid() & 0x7];
    sem->sem_wait_list[sem->sem_head_index] = t;
    sem->sem_head_index = (sem->sem_head_index + 1) % MAX_WAIT_THREAD;
	sem->sem_wait_count++;
    // writef("tcb %x wait begin\n", t->tcb_id);
    syscall_set_thread_status(0, THREAD_NOT_RUNNABLE);
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
    syscall_yield();
    return 0;
}

/* 
 * results:
 * P操作，若信号量小于等于0则报错(E_SEM_AGAIN)
 */
int sem_trywait(sem_t *sem){
    if((sem->sem_envid != env->env_id) && (sem->sem_shared == SEM_PROCESS_PRIVATE)){
        return -E_SEM_NOT_FOUND;  // 如果sem不是当前进程创建的，而且不是进程间公用的，返回-E_SEM_NOT_FOUND
    }
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_DISABLED);
    if(sem -> sem_status == SEM_FREE){
        syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
        return -E_SEM_FREE;
    }
    if(sem->sem_value <= 0){
        syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
        return -E_SEM_AGAIN;
    }
    sem->sem_value--;
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
    return 0;
}

/* 
 * results:
 * V操作，信号量+1
 */
int sem_post(sem_t *sem){
    if((sem->sem_envid != env->env_id) && (sem->sem_shared == SEM_PROCESS_PRIVATE)){
        return -E_SEM_NOT_FOUND;  // 如果sem不是当前进程创建的，而且不是进程间公用的，返回-E_SEM_NOT_FOUND
    }
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_DISABLED);
    if(sem -> sem_status == SEM_FREE){
        syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
        return -E_SEM_FREE;
    }
    if(sem->sem_wait_count == 0){
        sem->sem_value++;
        syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
        return 0;
    }
    struct Tcb *t;
    t = sem->sem_wait_list[sem->sem_tail_index];
    sem->sem_tail_index = (sem->sem_tail_index + 1) % MAX_WAIT_THREAD;
    sem->sem_wait_count--;
    // writef("tcb %x wait end\n", t->tcb_id);
    syscall_set_thread_status(t->tcb_id, THREAD_RUNNABLE);
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
    return 0;
}

/* 
 * parameter meanings:
 * sem_t *sem: 被查询的信号量
 * int *sval: 若没有线程等待，保存value；否则返回负的等待线程数
 * 
 * results:
 * 该函数返回当前信号量的值
 */
int sem_getvalue(sem_t *sem, int *sval){
    if(sem -> sem_status == SEM_FREE){
        return -E_SEM_FREE;
    }
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_DISABLED);
    if(sval != 0) {
        if(sem->sem_wait_count == 0){
            *sval = sem->sem_value;
        } else {
            *sval = -sem->sem_wait_count;
        }
    }
    syscall_set_thread_interrupt(0, PTHREAD_INTERRUPT_ON);
    return 0;
}
