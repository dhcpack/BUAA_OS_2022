#include "lib.h"

void *test1(void *args) {
	sem_t *sem = (sem_t *)((u_int *)args)[1];
	int a;
	int r;
	for (a = 1; a <= 2; ++a) {
		r = sem_wait(sem);
		user_assert(r == 0);
		writef("thread1 P time is %d\n", a);
	}
	writef("thread1 finish!\n");
}

void *test2(void *args) {
	sem_t *sem = (sem_t *)((u_int *)args)[1];
	int b;
	int r;
	for (b = 1; b <= 3; ++b) {
		r = sem_wait(sem);
		user_assert(r == 0);
		writef("thread2 P time is %d\n", b);
	}
	writef("thread2 finish\n");
}

void *test3(void *args) {
	sem_t *sem = (sem_t *)((u_int *)args)[1];
	int c;
	int r;
	for (c = 1; c <= 5; ++c) {
		r = sem_wait(sem);
		user_assert(r == 0);
		writef("thread3 P time is %d\n", c);
	}
	writef("thread3 finish\n");
	int value;
	r = sem_getvalue(sem, &value);
	user_assert(r == 0);
	user_assert(value == 0);
	writef("testpoint accepted!\n");
}

void umain()
{
	u_int a[2];
	a[0] = pthread_self();
	pthread_t thread1;
	pthread_t thread2;
	pthread_t thread3;

	int r, value;
	sem_t sem;
	sem_init(&sem, 0, 1);
	r = sem_getvalue(&sem, &value);
	user_assert(r == 0);
	writef("sem create succeed\n");

	a[1] = &sem;
	r += pthread_create(&thread1, NULL, test1, (void *)a);
	r += pthread_create(&thread2, NULL, test2, (void *)a);
	r += pthread_create(&thread3, NULL, test3, (void *)a);
	user_assert(r == 0);
	writef("thread create succeed!\n");

	int i;
	for (i = 1; i < 10; ++i) {
		while (value >= 0) {
			sem_getvalue(&sem, &value);
		}
		writef("umain V time is %d\n", i);
		sem_post(&sem);
		syscall_yield();
	}
}
