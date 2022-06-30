#include "lib.h"

void *th1(void *args) {
    pthread_mutex_t *errorchecklock = (pthread_mutex_t *)((u_int *)args)[0];
    pthread_mutex_t *recursivelock = (pthread_mutex_t *)((u_int *)args)[1];
	int r;

    r = pthread_mutex_lock(recursivelock);
    user_assert(r == 0);
    writef("recursive_lock lock once in thread 1\n");
    syscall_yield();

    r = pthread_mutex_lock(recursivelock);
    user_assert(r == 0);
	writef("lock tail index is %d\n", recursivelock->sem->sem_tail_index);
	writef("lock head index is %d\n", recursivelock->sem->sem_head_index);
    writef("recursive_lock lock twice in thread 1\n");
    syscall_yield();

    r = pthread_mutex_unlock(recursivelock);
    user_assert(r == 0);
	writef("lock tail index is %d\n", recursivelock->sem->sem_tail_index);
	writef("lock head index is %d\n", recursivelock->sem->sem_head_index);
    writef("recursive_lock unlock once in thread 1\n");
    syscall_yield();

    r = pthread_mutex_unlock(recursivelock);
    user_assert(r == 0);
	writef("lock tail index is %d\n", recursivelock->sem->sem_tail_index);
	writef("lock head index is %d\n", recursivelock->sem->sem_head_index);
    writef("recursive_lock unlock twice in thread 1\n");
    syscall_yield();

    r = pthread_mutex_unlock(recursivelock);
    user_assert(r == -24);  // E_UNLOCK_FAIL
	writef("lock tail index is %d\n", recursivelock->sem->sem_tail_index);
	writef("lock head index is %d\n", recursivelock->sem->sem_head_index);
    writef("recursive lock check succeed\n");
    syscall_yield();

    r = pthread_mutex_lock(errorchecklock);
    user_assert(r == 0);
    writef("errorcheck_lock lock succeed\n");
    syscall_yield();

    r = pthread_mutex_lock(errorchecklock);
    user_assert(r == -25);   // deadlock
    writef("error check succeed\n");
    syscall_yield();

    r = pthread_mutex_unlock(errorchecklock);
    user_assert(r == 0);
    writef("errorcheck_lock unlock succeed\n");
    syscall_yield();

    writef("errorcheck lock check succeed\n");
    writef("testpoint accepted!\n");
    pthread_cancel(0x2000);
}

void *th2(void *args) {
    pthread_mutex_t *errorchecklock = (pthread_mutex_t *)((u_int *)args)[0];
    pthread_mutex_t *recursivelock = (pthread_mutex_t *)((u_int *)args)[1];
    int r;
    writef("thread 2 wants recursive lock\n");
	writef("lock tail index is %d\n", recursivelock->sem->sem_tail_index);
	writef("lock head index is %d\n", recursivelock->sem->sem_head_index);
    r = pthread_mutex_lock(recursivelock);
    writef("thread2 get recursive lock\n");
    user_assert(r == 0);
}

void umain()
{
	u_int a[2];
	pthread_t thread1;
	pthread_t thread2;

    pthread_mutex_t errorchecklock, recursivelock;
    pthread_attr_t attr1, attr2;
    pthread_mutexattr_settype(&attr1, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutexattr_settype(&attr2, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&errorchecklock, &attr1);
    pthread_mutex_init(&recursivelock, &attr2);
    writef("mutex get!\n");

    a[0] = &errorchecklock;
    a[1] = &recursivelock;

    int r = 0;
	r += pthread_create(&thread1, NULL, th1, (void *)a);
    syscall_yield();
    r += pthread_create(&thread2, NULL, th2, (void *)a);
	user_assert(r == 0);
	writef("thread create succeed!\n");
    while(1) {
        syscall_yield();
    }
}
