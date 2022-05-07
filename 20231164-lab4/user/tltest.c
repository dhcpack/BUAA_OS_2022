#include "lib.h"

void umain() {
//	printf("enter tltest\n");
	writef(
        "Smashing some kernel codes...\n"
        "If your implementation is correct, you may see some TOO LOW here:\n"
    );
//	printf("finish 1 writef\n");
    *(int *) KERNBASE = 0;
    writef("My mission completed!\n");
//	printf("finish 2 writef\n");
}
