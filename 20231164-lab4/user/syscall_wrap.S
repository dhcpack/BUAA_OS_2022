#include <asm/regdef.h>
#include <asm/cp0regdef.h>
#include <asm/asm.h>

/*** exercise 4.1 ***/
LEAF(msyscall)
    // TODO: you JUST need to execute a `syscall` instruction and return from msyscall
    syscall  // 调用syscall陷入内核态
	nop
	jr ra    // 跳转返回
    nop
END(msyscall)


