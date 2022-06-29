#include "lib.h"

void *th2(void *args)
{
    int i;
    for (i = 0; i < 200;i++){
        writef("thread2 alive\n");
        syscall_yield();
    }
}

void *th3(void *args)
{
    int r;
    u_int thread1 = ((u_int *)args)[0];
    u_int thread2 = ((u_int *)args)[1];
    r = pthread_cancel(thread1);
    user_assert(r == 0);
    writef("cancel test1 succeed\n");
    r = pthread_cancel(thread2);
    user_assert(r == 0);
    writef("cancel test2 succeed\n");
    writef("testpoint accepted!\n");
}

void umain()
{
    int r = 0;
    u_int ret = -23187;
    u_int thread1 = pthread_self();

    pthread_attr_t attr;
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_t thread2;
    pthread_t thread3;

    r += pthread_create(&thread2, &attr, th2, NULL);
    int args[2];
    args[0] = thread1;
    args[1] = thread2;
    r += pthread_create(&thread3, &attr, th3, (void *)args);
    user_assert(r == 0);
    writef("thread create succeed\n");

    int i;
    for (i = 0; i < 200;i++){
        writef("thread 1 alive\n");
        syscall_yield();
    }
}