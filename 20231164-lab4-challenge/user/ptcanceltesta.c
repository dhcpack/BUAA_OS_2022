#include "lib.h"

void *th2(void *args)
{
    u_int thread1 = 0x2000;
    u_int thread2 = 0x2001;
    u_int thread3 = 0x2002;
    int r, oldstate;
    r = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    user_assert(r == 0);
    user_assert(oldstate == PTHREAD_CANCEL_ENABLE);
    writef("thread2 switch to PTHREAD_CANCEL_DISABLE succeed\n");
    syscall_yield();

    r = pthread_cancel(thread3);
    user_assert(r == 0);
    writef("thread2 signal cancel to thread3\n");
    syscall_yield();

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
    pthread_testcancel();
    writef("this line should not output\n");
}

void *th3(void *args)
{
    int r;
    u_int thread1 = 0x2000;
    u_int thread2 = 0x2001;
    u_int thread3 = 0x2002;
    syscall_yield();
    r = pthread_cancel(thread1);
    user_assert(r == 0);
    writef("thread3 signal cancel to thread1\n");
    syscall_yield();
    pthread_cancel(thread2);
    writef("thread3 signal cancel to thread2\n");
    syscall_yield();
    pthread_testcancel();
    writef("this line should not output\n");
}

void umain()
{
    int r = 0;
    u_int thread1 = pthread_self();

    pthread_attr_t attr;
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_t thread2;
    pthread_t thread3;

    r += pthread_create(&thread2, &attr, th2, NULL);
    r += pthread_create(&thread3, &attr, th3, NULL);
    user_assert(r == 0);
    writef("thread create succeed\n");

    int oldtype;
    r = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);
    user_assert(r == 0);
    user_assert(oldtype == PTHREAD_CANCEL_DEFERRED);
    writef("thread1 switch to PTHREAD_CANCEL_ASYNCHRONOUS succeed\n");

    int i;
    for (i = 0; i < 5;i++){
        writef("thread 1 alive\n");
        syscall_yield();
    }
    writef("this line should not output\n");
}