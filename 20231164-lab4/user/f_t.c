#include "lib.h"
void umain() {
	int pid;
	pid = fork();

	if(pid == 0) {
		writef("this is child process\n");
	} else {
		writef("this is parent process\n");
	}
}
