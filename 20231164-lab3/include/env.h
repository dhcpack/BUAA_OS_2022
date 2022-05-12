/* See COPYRIGHT for copyright information. */

#ifndef _ENV_H_
#define _ENV_H_

#include "types.h"
#include "queue.h"
#include "trap.h"
#include "mmu.h" 

#define LOG2NENV	10
#define NENV		(1<<LOG2NENV)                // 进程的数量 最多是2**10
#define ENVX(envid)	((envid) & (NENV - 1))       // 由envid得到env数组下标，取envid的低十位
#define GET_ENV_ASID(envid) (((envid)>> 11)<<6)  // 由envid得到asid

// Values of env_status in struct Env
#define ENV_FREE	0              // 表明该进程是不活动的，即该进程控制块处于进程空闲链表中。
#define ENV_RUNNABLE		1      // 表明该进程处于执行状态或就绪状态，即其可能是正在运行的，也可能正在等待被调度。
#define ENV_NOT_RUNNABLE	2      // 表明该进程处于阻塞状态，处于该状态的进程需要在一定条件下变成就绪状态从而被CPU调度。（比如因进程通信阻塞时变为 ENV_NOT_RUNNABLE，收到信息后变回 ENV_RUNNABLE）

struct Env {
	struct Trapframe env_tf;        // Saved registers  Trapframe 结构体的定义在include/trap.h 中，在发生进程调度，或当陷入内核时，会将当时的进程上下文环境保存在env_tf变量中。
	LIST_ENTRY(Env) env_link;       // Free list  env_link 的机制类似于lab2中的pp_link, 使用它和env_free_list来构造空闲进程链表。
	u_int env_id;                   // Unique environment identifier  每个进程的env_id 都不一样，它是进程独一无二的标识符。
	u_int env_parent_id;            // env_id of this env's parent  进程是可以被其他进程创建的，创建本进程的进程称为父进程。此变量记录父进程的进程 id，进程之间通过此关联可以形成一棵进程树。
	u_int env_status;               // Status of the environment  当前进程的状态 取值只能是ENV_FREE, ENV_NOT_RUNNABLE,ENV_RUNNABLE
	Pde  *env_pgdir;                // Kernel virtual address of page dir 保存了该进程页目录的内核虚拟地址。
	u_int env_cr3;                  // physical address of page dir  保存了该进程页目录的物理地址。
	LIST_ENTRY(Env) env_sched_link; // env schedule link  构造调度队列
    u_int env_pri;                  // env priority  保存了该进程的优先级

	// Lab 4 IPC
	u_int env_ipc_value;            // data value sent to us 
	u_int env_ipc_from;             // envid of the sender  
	u_int env_ipc_recving;          // env is blocked receiving
	u_int env_ipc_dstva;		// va at which to map received page
	u_int env_ipc_perm;		// perm of page mapping received

	// Lab 4 fault handling
	u_int env_pgfault_handler;      // page fault state
	u_int env_xstacktop;            // top of exception stack

	// Lab 6 scheduler counts
	u_int env_runs;			        // number of times been env_run'ed
	u_int env_nop;                  // align to avoid mul instruction
};

LIST_HEAD(Env_list, Env);
extern struct Env *envs;		// All environments
extern struct Env *curenv;	        // the current env
extern struct Env_list env_sched_list[2]; // runnable env list

void env_init(void);
int env_alloc(struct Env **e, u_int parent_id);
void env_free(struct Env *);
void env_create_priority(u_char *binary, int size, int priority);
void env_create(u_char *binary, int size);
void env_destroy(struct Env *e);

int envid2env(u_int envid, struct Env **penv, int checkperm);
void env_run(struct Env *e);


// for the grading script
#define ENV_CREATE2(x, y) \
{ \
	extern u_char x[], y[]; \
	env_create(x, (int)y); \
}
#define ENV_CREATE_PRIORITY(x, y) \
{\
        extern u_char binary_##x##_start[]; \
        extern u_int binary_##x##_size;\
        env_create_priority(binary_##x##_start, \
                (u_int)binary_##x##_size, y);\
}
#define ENV_CREATE(x) \
{ \
	extern u_char binary_##x##_start[];\
	extern u_int binary_##x##_size; \
	env_create(binary_##x##_start, \
		(u_int)binary_##x##_size); \
}

#endif // !_ENV_H_


