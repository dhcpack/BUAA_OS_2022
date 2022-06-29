#include "lib.h"

void *th2(void *args)
{
    int r;
    u_int thread1 = ((u_int *)args)[0];
    u_int *ret;
    r = pthread_join(thread1, (void *)&ret);
    user_assert(r == 0);
    user_assert(*ret == -23187);
    writef("thread 2 join test succeed\n");
    syscall_yield();
}

void *th3(void *args)
{
    int r;
    u_int thread1 = ((u_int *)args)[0];
    u_int *ret;
    r = pthread_join(thread1, (void *)&ret);
    user_assert(r == 0);
    user_assert(*ret == -23187);
    writef("thread 3 join test succeed\n");
    int b = 0;
    while (b < 3) {
        ++b;
        writef("thread3 is %d\n", b);
        syscall_yield();
    }
    *ret = -99999;
    pthread_exit(ret);
    syscall_yield();
}

void *th4(void *args)
{
    int r;
    u_int thread3 = ((u_int *)args)[0];
    u_int *ret;
    r = pthread_join(thread3, (void *)&ret);
    user_assert(r == 0);
    user_assert(*ret == -99999);
    writef("thread 4 join test succeed\n");
    writef("test point accepted\n");
    syscall_yield();
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
    pthread_t thread4;

    // 创建线程2,3,4，线程2, 3 join线程1，线程4join线程3
    r += pthread_create(&thread2, &attr, th2, (void *)&thread1);
    writef("thread2 create succeed\n");
    r += pthread_create(&thread3, &attr, th3, (void *)&thread1);
    writef("thread3 create succeed\n");
    r += pthread_create(&thread4, &attr, th4, (void *)&thread3);
    writef("thread4 create succeed\n");
    user_assert(r == 0);

    int b = 0;
    while (b < 3) {
        ++b;
        writef("thread1 is %d\n", b);
        syscall_yield();
    }

    pthread_exit(&ret);
}