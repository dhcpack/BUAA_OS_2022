#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *
 *
 * Hints:
 *  1. The variable which is for counting should be defined as 'static'.
 *  2. Use variable 'env_sched_list', which is a pointer array.
 *  3. CANNOT use `return` statement!
 */
/*** exercise 3.15 ***/
void sched_yield(void)
{
    static int count = 0; // remaining time slices of current env
    static int point = 0; // current env_sched_list index
    static struct Tcb *t = NULL;

    if(count == 0 && t != NULL && t->tcb_status == THREAD_RUNNABLE && t->tcb_irq == PTHREAD_INTERRUPT_DISABLED) {
        count++;  // 线程关中断，再运行一个周期
    }

	if(count == 0 || t == NULL || t->tcb_status != THREAD_RUNNABLE) {
        if(t != NULL && t->tcb_status == THREAD_RUNNABLE){
            LIST_REMOVE(t, tcb_sched_link);
            // struct Tcb *tt = LIST_FIRST(&tcb_sched_list[point]);;
            // while (tt!=NULL)
            // {
            //     printf("tcb id is %x\n", tt->tcb_id);
            //     tt = LIST_NEXT(tt, tcb_sched_link);
            // }
            LIST_INSERT_TAIL(&tcb_sched_list[1 - point], t, tcb_sched_link);  // 在env_sched_list中操作节点要用env_sched_link
        }

        while(1) {
            while (LIST_EMPTY(&tcb_sched_list[point])) {
				point = 1 - point;
                // printf("change list\n");
            }
            t = LIST_FIRST(&tcb_sched_list[point]);
            if(t -> tcb_status == THREAD_FREE) {
                // printf("%d\n", t->tcb_id);
                LIST_REMOVE(t, tcb_sched_link);
            } else if(t->tcb_status == THREAD_NOT_RUNNABLE) {
                // printf("22\n");
                LIST_REMOVE(t, tcb_sched_link);
                LIST_INSERT_TAIL(&tcb_sched_list[1 - point], t, tcb_sched_link);
            } else {
                // printf("33\n");
				count = t->tcb_pri;
                // printf("switch to a new tcb\n");
                break;
            }
        }
    }

    count--;
    // printf("run tcb %x\n", t->tcb_id);
    thread_run(t);
}
