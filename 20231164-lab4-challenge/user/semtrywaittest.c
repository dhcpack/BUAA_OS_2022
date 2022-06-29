#include "lib.h"

void *trywaittest(void *args) {
	sem_t *sem = (sem_t *)((u_int *)args)[0];
	int a;
	int r;
    r = sem_trywait(sem);
    user_assert(r == 0);
    writef("try wait test1 succeed\n");
    r = sem_trywait(sem);
    user_assert(r == -18);  // define in include/error.h  #define E_SEM_AGAIN     18  // semaphore value <= 0
    writef("try wait test2 succeed\n");
    writef("testpoint accepted!\n");
}

void umain()
{
	u_int a;
	pthread_t thread1;

	sem_t sem;
	sem_init(&sem, 0, 1);
	a = &sem;
	int r, value;
	r = 0;
	r += pthread_create(&thread1, NULL, trywaittest, (void *)&a);
	user_assert(r == 0);
	writef("thread create succeed!\n");
}
