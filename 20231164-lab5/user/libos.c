#include "lib.h"
#include <mmu.h>
#include <env.h>
// 用户进程入口的C语言部分，负责完成执行用户程序umain前后的准备和清理工作
void
exit(void)
{
	//close_all();
	syscall_env_destroy(0);
}


struct Env *env;
// 在子进程第一次被调度的时候（当然这时还是在fork函数中）需要对 env 指针进行更新，使其仍指向当前进程的控制块。？？
void
libmain(int argc, char **argv)
{
	// set env to point at our env structure in envs[].
	env = 0;	// Your code here.
	//writef("xxxxxxxxx %x  %x  xxxxxxxxx\n",argc,(int)argv);
	int envid;
	envid = syscall_getenvid();
	envid = ENVX(envid);
	env = &envs[envid];
	// call user main routine
	umain(argc, argv);
	// exit gracefully
	exit();
	//syscall_env_destroy(0);
}
