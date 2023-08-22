	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */
	//复制kernel/swtch.S内容,用于切换线程执行的上下文
	//ra:Return Address Register:返回地址寄存器,存储函数调用返回的地址，即函数调用结束后的下一条指令的地址。
  	//sp:Stack Pointer Register:栈指针寄存器,指向当前线程的栈顶。
	//在汇编层面实现了保存当前线程的寄存器状态到一个结构体（old），然后从另一个结构体（new）中加载寄存器状态以切换到另一个线程的执行。sd:存储双字/ld:加载双字,用于存储和加载寄存器的值,可以在切换线程时保存当前线程上下文并回复新线程上下文,实现线程的无缝切换
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
        
        ret
