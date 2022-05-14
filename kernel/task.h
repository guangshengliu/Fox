#ifndef __TASK_H__
#define __TASK_H__

#include "memory.h"
#include "cpu.h"
#include "lib.h"
#include "ptrace.h"

#define KERNEL_CS 	(0x08)
#define	KERNEL_DS 	(0x10)

#define	USER_CS		(0x28)
#define USER_DS		(0x30)

#define CLONE_FS	(1 << 0)
#define CLONE_FILES	(1 << 1)
#define CLONE_SIGNAL	(1 << 2)

// stack size 32K
#define STACK_SIZE 32768

extern char _text;
extern char _etext;
extern char _data;
extern char _edata;
extern char _rodata;
extern char _erodata;
extern char _bss;
extern char _ebss;
extern char _end;

extern unsigned long kallsyms_addresses[] __attribute__((weak));
extern long kallsyms_syms_num __attribute__((weak));
extern long kallsyms_index[] __attribute__((weak));
extern char* kallsyms_names __attribute__((weak));

extern unsigned long _stack_start;

/*

*/

#define TASK_RUNNING		(1 << 0)
#define TASK_INTERRUPTIBLE	(1 << 1)
#define	TASK_UNINTERRUPTIBLE	(1 << 2)
#define	TASK_ZOMBIE		(1 << 3)	
#define	TASK_STOPPED		(1 << 4)

/*
*	内存空间分布结构体
*/


struct mm_struct
{
	pml4t_t *pgd;	//	内存页表指针，保持在CR3
	
	unsigned long start_code,end_code;		// 代码段空间
	unsigned long start_data,end_data;		// 数据段空间
	unsigned long start_rodata,end_rodata;	// 只读数据段空间
	unsigned long start_brk,end_brk;		// 动态内存分配区(堆区域)
	unsigned long start_stack;				// 应用层栈基地址
};

/*
*	进程切换时状态信息
*/

struct thread_struct
{
	unsigned long rsp0;			// 内核层栈基地址

	unsigned long rip;			// 内核层代码指针
	unsigned long rsp;			// 内核层当前栈指针

	unsigned long fs;			// FS段寄存器
	unsigned long gs;			// GS段寄存器

	unsigned long cr2;			// CR2控制寄存器
	unsigned long trap_nr;		// 产生异常的异常号
	unsigned long error_code;	// 异常的错误码
};

/*
*	PCB进程控制块
*/

struct task_struct
{
	volatile long state;	// 进程状态：运行态，停止态，可中断态等
	unsigned long flags;	// 进程标志：进程，线程，内核线程
	long preempt_count;		// 自旋锁计算量
	long signal;	// 进程持有的信号
	long cpu_id;		//CPU ID

	struct mm_struct *mm;	// 内存空间分布结构体，记录内存页表和程序段信息
	struct thread_struct *thread;	// 进程切换时保留的状态信息
	struct List list;		// 双向链表，连接各个进程控制结构体
	// 进程地址空间范围
	unsigned long addr_limit;	/*0x0000,0000,0000,0000 - 0x0000,7fff,ffff,ffff user*/
					/*0xffff,8000,0000,0000 - 0xffff,ffff,ffff,ffff kernel*/

	long pid;		// 进程ID号
	long priority;	// 进程优先级
	long vrun_time;	// 进程期望运行时间
};

//	struct task_struct->flags:
#define PF_KTHREAD	(1UL << 0)
//	Weather can be schedule
#define NEED_SCHEDULE	(1UL << 1)

/*
*	进程控制结构体与进程的内核层栈空间共用空间
*/

union task_union
{
	struct task_struct task;
	unsigned long stack[STACK_SIZE / sizeof(unsigned long)];
}__attribute__((aligned (8)));	//8Bytes align

#define INIT_TASK(tsk)	\
{			\
	.state = TASK_UNINTERRUPTIBLE,		\
	.flags = PF_KTHREAD,		\
	.preempt_count = 0,		\
	.signal = 0,		\
	.cpu_id = 0,		\
	.mm = &init_mm,			\
	.thread = &init_thread,		\
	.addr_limit = 0xffff800000000000,	\
	.pid = 0,			\
	.priority = 2,		\
	.vrun_time = 0		\
}

/*
*	IA-32e模式下的TSS结构
*/

struct tss_struct
{
	unsigned int  reserved0;
	unsigned long rsp0;
	unsigned long rsp1;
	unsigned long rsp2;
	unsigned long reserved1;
	unsigned long ist1;
	unsigned long ist2;
	unsigned long ist3;
	unsigned long ist4;
	unsigned long ist5;
	unsigned long ist6;
	unsigned long ist7;
	unsigned long reserved2;
	unsigned short reserved3;
	unsigned short iomapbaseaddr;
}__attribute__((packed));

// INIT_TSS初始化宏
#define INIT_TSS \
{	.reserved0 = 0,	 \
	.rsp0 = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)),	\
	.rsp1 = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)),	\
	.rsp2 = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)),	\
	.reserved1 = 0,	 \
	.ist1 = 0xffff800000007c00,	\
	.ist2 = 0xffff800000007c00,	\
	.ist3 = 0xffff800000007c00,	\
	.ist4 = 0xffff800000007c00,	\
	.ist5 = 0xffff800000007c00,	\
	.ist6 = 0xffff800000007c00,	\
	.ist7 = 0xffff800000007c00,	\
	.reserved2 = 0,	\
	.reserved3 = 0,	\
	.iomapbaseaddr = 0	\
}

/*
*	得到当前进程struct task_struct结构体的基地址
*/

static inline struct task_struct * get_current()
{
	struct task_struct * current = NULL;
	// 将数值32767取反，再将所得结果与栈指针寄存器RSP的值执行逻辑与计算
	__asm__ __volatile__ ("andq %%rsp,%0	\n\t":"=r"(current):"0"(~32767UL));
	return current;
}

#define current get_current()

#define GET_CURRENT			\
	"movq	%rsp,	%rbx	\n\t"	\
	"andq	$-32768,%rbx	\n\t"

/*
*	prev进程通过调用switch_to模块来保存RSP寄存器的当前值
*	指定切换回prev进程时的RIP寄存器值，默认为1：处
*	将next进程的栈指针恢复到RSP寄存器中，再把next进程执行现场的RIP寄存器值压入next进程的内核层栈空间
*	借助JMP指令执行_switch_to函数
*/

#define switch_to(prev,next)			\
do{							\
	__asm__ __volatile__ (	"pushq	%%rbp	\n\t"	\
				"pushq	%%rax	\n\t"	\
				"movq	%%rsp,	%0	\n\t"	\
				"movq	%2,	%%rsp	\n\t"	\
				"leaq	1f(%%rip),	%%rax	\n\t"	\
				"movq	%%rax,	%1	\n\t"	\
				"pushq	%3		\n\t"	\
				"jmp	__switch_to	\n\t"	\
				"1:	\n\t"	\
				"popq	%%rax	\n\t"	\
				"popq	%%rbp	\n\t"	\
				:"=m"(prev->thread->rsp),"=m"(prev->thread->rip)		\
				:"m"(next->thread->rsp),"m"(next->thread->rip),"D"(prev),"S"(next)	\
				:"memory"		\
				);			\
}while(0)

/*

*/

unsigned long do_fork(struct pt_regs * regs, unsigned long clone_flags, unsigned long stack_start, unsigned long stack_size);
void task_init();

extern void ret_system_call(void);
extern void system_call(void);

extern struct task_struct *init_task[NR_CPUS];
extern union task_union init_task_union;
extern struct mm_struct init_mm;
extern struct thread_struct init_thread;

extern struct tss_struct init_tss[NR_CPUS];

#endif
