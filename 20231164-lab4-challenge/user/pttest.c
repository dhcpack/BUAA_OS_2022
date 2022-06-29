#include "lib.h"

void *pttest(void *arg){
	int *a = ((int *)arg)[0];
	char *s = ((int *)arg)[1];
	writef("a = %d, s = %s\n", *a, s);
	user_assert(*a == 23187);
	user_assert(strcmp(s, "test shared information") == 0);  // 第一次检查
	writef("shared memory check1 succeed\n");
	s[4] = "\0";
	*a = 0;
	writef("num set in pttest\n");

	while (1) {
		if(*a != 0){
			break;
		}
		writef("a addr is %x\n", a);
	}  // 第三次检查
	writef("shared memory check3 succeed\n");
	writef("testpoint accepted\n");
}
void umain() {
	int r;
	int num = 23187;
	char string[] = "test shared information";  // 测试局部变量的共享
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_setdetachstate(&attr, JOINABLE_STATE);
	int args[3];
	args[0] = &num;  // 传递变量地址，检查地址空间是否共享
	args[1] = string;

	r = pthread_create(&thread, &attr, pttest, (void *)args);
	user_assert(r == 0);
	writef("thread create succeed\n");
	writef("num addr is %x\n", &num);

	while(1){
		if(num != 23187){
			break;
		}
	}
	writef("num is %d, string is %s\n", num, string);
	user_assert(strcmp(string, "test") == 0);  // 第二次检查
	writef("shared memory check2 succeed\n");
	num = 1;
	writef("num reset in umain\n");
}