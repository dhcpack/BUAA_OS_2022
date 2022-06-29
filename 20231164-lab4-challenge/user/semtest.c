#include "lib.h"

void *test1(void *args) {
	sem_t *sem = (sem_t *)((u_int *)args)[1];
	int a;
	int r;
	for (a = 0; a < 2; ++a) {
		r = sem_wait(sem);
		user_assert(r == 0);
		writef("son1 P! now a is %d\n", a);
	}
	writef("thread1 finish!\n");
}

void *test2(void *args) {
	sem_t *sem = (sem_t *)((u_int *)args)[1];
	int b;
	int r;
	for (b = 0; b < 3; ++b) {
		r = sem_wait(sem);
		user_assert(r == 0);
		writef("son2 P! now b is %d\n", b);
	}
	writef("thread2 finish\n");
}

void *test3(void *args) {
	sem_t *sem = (sem_t *)((u_int *)args)[1];
	int c;
	int r;
	for (c = 0; c < 5; ++c) {
		r = sem_wait(sem);
		user_assert(r == 0);
		writef("son3 P! now c is %d\n", c);
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

	sem_t sem;
	sem_init(&sem, 0, 1);
	a[1] = &sem;
	int r, value;
	r = 0;
	r += pthread_create(&thread1, NULL, test1, (void *)a);
	r += pthread_create(&thread2, NULL, test2, (void *)a);
	r += pthread_create(&thread3, NULL, test3, (void *)a);
	user_assert(r == 0);
	writef("thread create succeed!\n");

	r = sem_getvalue(&sem, &value);
	user_assert(r == 0);
	writef("sem create succeed\n");

	int i = 0;
	for (i = 0; i < 9; ++i) {
		while (value >= 0) {
			sem_getvalue(&sem, &value);
		}
		writef("father post!\n");
		sem_post(&sem);
		syscall_yield();
	}
}
