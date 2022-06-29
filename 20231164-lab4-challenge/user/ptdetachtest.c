#include "lib.h"

void *th2(void *args)
{
    writef("thread2 is working\n");
    int i;
    for (i = 0; i < 2;i++){
        syscall_yield();
    }
}

void *th3(void *args)
{
    int r;
    u_int thread1 = ((u_int *)args)[0];
    u_int thread2 = ((u_int *)args)[1];
    u_int *ret;
    r = pthread_join(thread1, (void *)&ret);  // 测试pthread_detach()
    user_assert(r == -15);  // define in include\error.h: (#define E_THREAD_JOIN_FAIL  15  // detached thread cannot join)
    writef("detach test1 succeed\n");
    r = pthread_join(thread2, (void *)&ret);  // 测试初始设置是否成功
    user_assert(r == -15); 
    writef("detach test2 succeed\n");
    writef("testpoint accepted!\n");
}

void umain()
{
    int r = 0;
    u_int ret = -23187;
    u_int thread1 = pthread_self();
    pthread_detach(thread1);

    pthread_attr_t attr;
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t thread2;
    pthread_t thread3;

    r += pthread_create(&thread2, &attr, th2, NULL);
    int args[2];
    args[0] = thread1;
    args[1] = thread2;
    r += pthread_create(&thread3, NULL, th3, (void *)args);
    user_assert(r == 0);
    writef("thread create succeed\n");

    int i;
    for (i = 0; i < 2;i++){
        syscall_yield();
    }
}