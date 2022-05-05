#include <env.h>
#include <pmap.h>
#include <printf.h>

int count = 0;
void sched_yield(void)
{
    static struct Env *e = NULL;

    count++;
    int max_pri = 0;
    struct Env *max_pri_e;

    if (e != NULL) {
        u_int func_1 = ((e->env_pri & 0xff00) >> 8);
        u_int pri = (e->env_pri & 0xff);
        if(pri  > func_1) {
            pri = pri - func_1;
        } else {
            pri = 0;
        }
        e->env_pri = (((e->env_pri) & 0xffffff00) | pri);
    }
    LIST_FOREACH(e, env_sched_list, env_sched_link) {
        u_int func_2 = ((e->env_pri & 0xff0000) >> 16);
        if(func_2 == count) {
            e->time_to_run = ((e->env_pri & 0xff000000) >> 24);
            e->env_status = ENV_NOT_RUNNABLE;
        } else if(e->time_to_run != 0) {
            e->time_to_run--;
            if(e->time_to_run == 0) {
                e->env_status = ENV_RUNNABLE;
            }
        }

        if((e->env_pri & 0xff) > max_pri && e->env_status == ENV_RUNNABLE) {
            max_pri_e = e;
            max_pri = (e->env_pri & 0xff);
        }
    }
    env_run(max_pri_e);
}