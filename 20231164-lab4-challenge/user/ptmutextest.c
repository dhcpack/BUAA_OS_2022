#include "lib.h"

void* public(void *args){
	pthread_mutex_t *mutex = (pthread_mutex_t *)((u_int *)args)[0];
    u_int times = ((u_int *)args)[1];
    int *number = ((u_int *)args)[2];
	int i = 0;
	while (i < times) {
		i++;
		pthread_mutex_lock(mutex);
		*number = *number + 1;
		if(times == 2){
			writef("thread 1 time is %d, number is %d\n", i, *number);
		} else if (times == 3) {
			writef("thread 2 time is %d, number is %d\n", i, *number);
		} else {
			writef("thread 3 time is %d, number is %d\n", i, *number);
		}
		pthread_mutex_unlock(mutex);
		syscall_yield();
	}
}

void umain()
{
    u_int a[3], b[3], c[3];
    int number = 0;
    pthread_t thread1;
	pthread_t thread2;
	pthread_t thread3;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
	writef("mutex get!\n");
	a[0] = &mutex; b[0] = &mutex; c[0] = &mutex;
    a[1] = 2; b[1] = 3; c[1] = 5;
    a[2] = &number; b[2] = &number; c[2] = &number;

    int r = 0;
	r += pthread_create(&thread1, NULL, public, (void *)a);
	r += pthread_create(&thread2, NULL, public, (void *)b);
	r += pthread_create(&thread3, NULL, public, (void *)c);
	user_assert(r == 0);
	writef("thread create succeed!\n");
}
