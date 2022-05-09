#include <asm/regdef.h>
#include <asm/cp0regdef.h>
#include <asm/asm.h>
#include <stackframe.h>
#include <unistd.h>

/*** exercise 4.2 ***/
// 执行syscall调用的函数，此时程序处于内核态
NESTED(handle_sys,TF_SIZE, sp)          // 这里的栈指针是内核状态下的栈指针
    SAVE_ALL                            // Macro used to save trapframe  将用户态的运行现场保存到了内核空间
    CLI                                 // Clean Interrupt Mask
    nop
    .set at                             // Resume use of $at

    // TODO: Fetch EPC from Trapframe, calculate a proper value and store it back to trapframe.
    lw      t0, TF_EPC(sp)
    lw      t1, TF_CAUSE(sp)
    lui     t2, 0x8000
    and     t1, t1, t2
    bnez    t1, IS_BD      // 判断CP0_CAUSE寄存器最高位是否为1，若为1则为分支延迟槽，EPC不加4
    nop
    addiu   t0, t0, 4
IS_BD:
    sw      t0, TF_EPC(sp)

    // TODO: Copy the syscall number into $a0.
    lw      a0, TF_REG4(sp)

    // 得到sys_*函数的入口地址
    addiu   a0, a0, -__SYSCALL_BASE     // a0 <- relative syscall number
    sll     t0, a0, 2                   // t0 <- relative syscall number times 4
    la      t1, sys_call_table          // t1 <- syscall table base
    addu    t1, t1, t0                  // t1 <- table entry of specific syscall
    lw      t2, 0(t1)                   // t2 <- function entry of specific syscall  // 得到sys_*函数的入口地址


    // 将参数从Trap Frame(user stack)取出来
    lw      t0, TF_REG29(sp)            // t0 <- user's stack pointer
    lw      t3, 16(t0)                  // t3 <- the 5th argument of msyscall
    lw      t4, 20(t0)                  // t4 <- the 6th argument of msyscall

    // 放到当前环境中(kernel stack)
    // TODO: Allocate a space of six arguments on current kernel stack and copy the six arguments to proper location
    lw      a0, TF_REG4(sp)             // 从Trap Frame寄存器拷贝到当前寄存器
    lw      a1, TF_REG5(sp)
    lw      a2, TF_REG6(sp)
    lw      a3, TF_REG7(sp)
    addiu   sp, sp, -24                 // 申请空间时减sp指针
    sw      t3, 16(sp)
    sw      t4, 20(sp)
    
    // 调用sys_*函数
    jalr    t2                          // Invoke sys_* function
    nop

    // TODO: Resume current kernel stack
    addiu   sp, sp, 24

    sw      v0, TF_REG2(sp)             // Store return value of function sys_* (in $v0) into trapframe

    j       ret_from_exception          // Return from exeception
    nop
END(handle_sys)

sys_call_table:                         // Syscall Table
    .align 2
    .word sys_putchar
    .word sys_getenvid
    .word sys_yield
    .word sys_env_destroy
    .word sys_set_pgfault_handler
    .word sys_mem_alloc
    .word sys_mem_map
    .word sys_mem_unmap
    .word sys_env_alloc
    .word sys_set_env_status
    .word sys_set_trapframe
    .word sys_panic
    .word sys_ipc_can_send
    .word sys_ipc_recv
    .word sys_cgetc

