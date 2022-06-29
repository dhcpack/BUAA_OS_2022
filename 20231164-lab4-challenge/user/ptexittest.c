#include "lib.h"

void *ptexittest(void *args) {
    int ret = ((int *)args)[0];
    writef("thread is exit with %d\n", ret);
    pthread_exit(&ret);
}

void umain() {
    int r;
    pthread_t thread;
    int args = -23187;

    r = pthread_create(&thread, NULL, ptexittest, (void *)&args);
    user_assert(r == 0);
    writef("thread create succeed!\n");

    while ((int)env->env_threads[thread & 0x7].tcb_status != ENV_FREE) {
        writef("thread status is %d\n", (int)env->env_threads[thread & 0x7].tcb_status);
    }
    user_assert(*((int *)env->env_threads[thread & 0x7].tcb_exit_ptr) == args);
    writef("testpoint accepted!\n");
}
