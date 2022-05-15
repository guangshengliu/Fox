#include "task.h"
#include "ptrace.h"
#include "lib.h"
#include "memory.h"
#include "linkage.h"
#include "unistd.h"
#include "gate.h"
#include "schedule.h"
#include "printk.h"
#include "SMP.h"

union task_union init_task_union __attribute__((__section__ (".data.init_task"))) = {INIT_TASK(init_task_union.task)};

struct task_struct *init_task[NR_CPUS] = {&init_task_union.task,0};

struct mm_struct init_mm = {0};

struct thread_struct init_thread = 
{
	.rsp0 = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)),
	.rsp = (unsigned long)(init_task_union.stack + STACK_SIZE / sizeof(unsigned long)),
	.fs = KERNEL_DS,
	.gs = KERNEL_DS,
	.cr2 = 0,
	.trap_nr = 0,
	.error_code = 0
};

struct tss_struct init_tss[NR_CPUS] = { [0 ... NR_CPUS-1] = INIT_TSS };

/*
*	内嵌系统调用在应用层部分的核心程序
*	LEA取得sysexit_return_address的地址，存入RDX寄存器
*	RCX寄存器保存着应用层的当前栈指针，RAX寄存器是系统调用API的向量号
*	执行结束后，执行结果保存在变量ret
*/

void user_level_function()
{
	int errno = 0;
	char string[]="Hello World!\n";

	color_printk(RED,BLACK,"user_level_function task is running\n");

//	call sys_putstring

	__asm__ __volatile__	(	"pushq	%%r10	\n\t"
					"pushq	%%r11	\n\t"
					"leaq	sysexit_return_address(%%rip),	%%r10	\n\t"
					"movq	%%rsp,	%%r11		\n\t"
					"sysenter			\n\t"
					"sysexit_return_address:	\n\t"
					"xchgq	%%rdx,	%%r10	\n\t"
					"xchgq	%%rcx,	%%r11	\n\t"
					"popq	%%r11	\n\t"
					"popq	%%r10	\n\t"
					:"=a"(errno)
					:"0"(__NR_putstring),"D"(string)
					:"memory");	

	color_printk(RED,BLACK,"user_level_function task called sysenter,errno:%d\n",errno);

	while(1);
}

/*
*	搭建应用程序执行环境
*/

unsigned long do_execve(struct pt_regs * regs)
{
	unsigned long addr = 0x800000;
	unsigned long * tmp;
	unsigned long * virtual = NULL;
	struct Page * p = NULL;
	
	//regs->rdx = 0x800000;	//RIP
	//regs->rcx = 0xa00000;	//RSP
	regs->r10 = 0x800000;	//RIP
	regs->r11 = 0xa00000;	//RSP	
	regs->rax = 1;	
	regs->ds = 0;
	regs->es = 0;
	color_printk(RED,BLACK,"do_execve task is running\n");

	Global_CR3 = Get_gdt();

	tmp = Phy_To_Virt((unsigned long *)((unsigned long)Global_CR3 & (~ 0xfffUL)) + ((addr >> PAGE_GDT_SHIFT) & 0x1ff));

	virtual = kmalloc(PAGE_4K_SIZE,0);
	set_mpl4t(tmp,mk_mpl4t(Virt_To_Phy(virtual),PAGE_USER_GDT));

	tmp = Phy_To_Virt((unsigned long *)(*tmp & (~ 0xfffUL)) + ((addr >> PAGE_1G_SHIFT) & 0x1ff));
	virtual = kmalloc(PAGE_4K_SIZE,0);
	set_pdpt(tmp,mk_pdpt(Virt_To_Phy(virtual),PAGE_USER_Dir));

	tmp = Phy_To_Virt((unsigned long *)(*tmp & (~ 0xfffUL)) + ((addr >> PAGE_2M_SHIFT) & 0x1ff));
	p = alloc_pages(ZONE_NORMAL,1,PG_PTable_Maped);
	set_pdt(tmp,mk_pdt(p->PHY_address,PAGE_USER_Page));

	flush_tlb();
	
	if(!(current->flags & PF_KTHREAD))
		current->addr_limit = 0xffff800000000000;

	memcpy(user_level_function,(void *)0x800000,1024);
	
	return 1;
}

/*
*	将init内核线程转化为应用程序
*/

unsigned long init(unsigned long arg)
{
	struct pt_regs *regs;

	color_printk(RED,BLACK,"init task is running,arg:%#018lx\n",arg);

	current->thread->rip = (unsigned long)ret_system_call;
	current->thread->rsp = (unsigned long)current + STACK_SIZE - sizeof(struct pt_regs);
	regs = (struct pt_regs *)current->thread->rsp;
	current->flags = 0;

	__asm__	__volatile__	(	"movq	%1,	%%rsp	\n\t"
					"pushq	%2		\n\t"
					"jmp	do_execve	\n\t"
					::"D"(regs),"m"(current->thread->rsp),"m"(current->thread->rip):"memory");

	return 1;
}

/*
*	创建进程控制结构体并完成进程运行前的初始化工作
*/

unsigned long do_fork(struct pt_regs * regs, unsigned long clone_flags, unsigned long stack_start, unsigned long stack_size)
{
	struct task_struct *tsk = NULL;
	struct thread_struct *thd = NULL;
	struct Page *p = NULL;
	
	color_printk(WHITE,BLACK,"alloc_pages,bitmap:%#018lx\n",*memory_management_struct.bits_map);

	p = alloc_pages(ZONE_NORMAL,1,PG_PTable_Maped | PG_Kernel);

	color_printk(WHITE,BLACK,"alloc_pages,bitmap:%#018lx\n",*memory_management_struct.bits_map);

	tsk = (struct task_struct *)Phy_To_Virt(p->PHY_address);
	color_printk(WHITE,BLACK,"struct task_struct address:%#018lx\n",(unsigned long)tsk);

	memset(tsk,0,sizeof(*tsk));
	*tsk = *current;

	list_init(&tsk->list);

	tsk->priority = 2;
	tsk->pid++;	
	tsk->preempt_count = 0;
	tsk->cpu_id = SMP_cpu_id();
	tsk->state = TASK_UNINTERRUPTIBLE;

	// 初始化thread_struct
	thd = (struct thread_struct *)(tsk + 1);
	memset(thd,0,sizeof(*thd));
	tsk->thread = thd;	
	// 制造进程执行现场
	memcpy(regs,(void *)((unsigned long)tsk + STACK_SIZE - sizeof(struct pt_regs)),sizeof(struct pt_regs));

	thd->rsp0 = (unsigned long)tsk + STACK_SIZE;
	thd->rip = regs->rip;
	thd->rsp = (unsigned long)tsk + STACK_SIZE - sizeof(struct pt_regs);
	thd->fs = KERNEL_DS;
	thd->gs = KERNEL_DS;
	// 判断目标进程PF_KTHREAD标志位
	// PF_KTHREAD=1则该进程运行于应用层
	// 程序入口地址设置在ret_from_intr地址处，否则设置在kernel_thread_func地址处
	if(!(tsk->flags & PF_KTHREAD))
		thd->rip = regs->rip = (unsigned long)ret_system_call;

	tsk->state = TASK_RUNNING;
	insert_task_queue(tsk);

	return 1;
}

/*
*	释放进程控制块
*/

unsigned long do_exit(unsigned long code)
{
	color_printk(RED,BLACK,"exit task is running,arg:%#018lx\n",code);
	while(1);
}


extern void kernel_thread_func(void);
__asm__ (
"kernel_thread_func:	\n\t"
"	popq	%r15	\n\t"
"	popq	%r14	\n\t"	
"	popq	%r13	\n\t"	
"	popq	%r12	\n\t"	
"	popq	%r11	\n\t"	
"	popq	%r10	\n\t"	
"	popq	%r9	\n\t"	
"	popq	%r8	\n\t"	
"	popq	%rbx	\n\t"	
"	popq	%rcx	\n\t"	
"	popq	%rdx	\n\t"	
"	popq	%rsi	\n\t"	
"	popq	%rdi	\n\t"	
"	popq	%rbp	\n\t"	
"	popq	%rax	\n\t"	
"	movq	%rax,	%ds	\n\t"
"	popq	%rax		\n\t"
"	movq	%rax,	%es	\n\t"
"	popq	%rax		\n\t"
"	addq	$0x38,	%rsp	\n\t"
/////////////////////////////////
"	movq	%rdx,	%rdi	\n\t"
"	callq	*%rbx		\n\t"
"	movq	%rax,	%rdi	\n\t"
"	callq	do_exit		\n\t"
);

/*
*	操作系统创建进程函数
*/

int kernel_thread(unsigned long (* fn)(unsigned long), unsigned long arg, unsigned long flags)
{
	struct pt_regs regs;
	memset(&regs,0,sizeof(regs));
	// RBX寄存器保存程序入口地址
	regs.rbx = (unsigned long)fn;
	// RDX寄存器保存进程创建者传入的参数
	regs.rdx = (unsigned long)arg;

	regs.ds = KERNEL_DS;
	regs.es = KERNEL_DS;
	regs.cs = KERNEL_CS;
	regs.ss = KERNEL_DS;
	regs.rflags = (1 << 9);
	// RIP寄存器保存着一段引导程序
	regs.rip = (unsigned long)kernel_thread_func;

	return do_fork(&regs,flags,0,0);
}

/*
*	切换进程函数
*/

void __switch_to(struct task_struct *prev,struct task_struct *next)
{
	unsigned int color = 0;
	// 获取next进程的内核层栈基地址
	init_tss[SMP_cpu_id()].rsp0 = next->thread->rsp0;
	// 将上述地址设置到TSS结构体对应的成员变量中
	//set_tss64(TSS64_Table,init_tss[0].rsp0, init_tss[0].rsp1, init_tss[0].rsp2, init_tss[0].ist1, init_tss[0].ist2, init_tss[0].ist3, init_tss[0].ist4, init_tss[0].ist5, init_tss[0].ist6, init_tss[0].ist7);
	__asm__ __volatile__("movq	%%fs,	%0 \n\t":"=a"(prev->thread->fs));
	__asm__ __volatile__("movq	%%gs,	%0 \n\t":"=a"(prev->thread->gs));
	// 将next进程保护的FS与GS段寄存器值还原
	__asm__ __volatile__("movq	%0,	%%fs \n\t"::"a"(next->thread->fs));
	__asm__ __volatile__("movq	%0,	%%gs \n\t"::"a"(next->thread->gs));

	wrmsr(0x175,next->thread->rsp0);

	if(SMP_cpu_id() == 0)
		color = WHITE;
	else
		color = YELLOW;

	color_printk(color,BLACK,"prev->thread->rsp0:%#018lx\t",prev->thread->rsp0);
	color_printk(color,BLACK,"prev->thread->rsp :%#018lx\n",prev->thread->rsp);
	color_printk(color,BLACK,"next->thread->rsp0:%#018lx\t",next->thread->rsp0);
	color_printk(color,BLACK,"next->thread->rsp :%#018lx\n",next->thread->rsp);
	color_printk(color,BLACK,"CPUID:%#018lx\n",SMP_cpu_id());
}

/*
*	补全系统第一个进程控制结构体中未赋值的成员变量
*	设置内核层栈基地址
*/

void task_init()
{
	init_mm.pgd = (pml4t_t *)Global_CR3;

	init_mm.start_code = memory_management_struct.start_code;
	init_mm.end_code = memory_management_struct.end_code;

	init_mm.start_data = (unsigned long)&_data;
	init_mm.end_data = memory_management_struct.end_data;

	init_mm.start_rodata = (unsigned long)&_rodata; 
	init_mm.end_rodata = (unsigned long)&_erodata;

	init_mm.start_brk = 0;
	init_mm.end_brk = memory_management_struct.end_brk;

	init_mm.start_stack = _stack_start;

	wrmsr(0x174,KERNEL_CS);
	wrmsr(0x175,current->thread->rsp0);
	wrmsr(0x176,(unsigned long)system_call);

//	init_thread,init_tss
	init_tss[SMP_cpu_id()].rsp0 = init_thread.rsp0;

	list_init(&init_task_union.task.list);
	// 创建第二个进程，名为init
	kernel_thread(init,10,CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
	init_task_union.task.preempt_count = 0;
	// init切换成运行态
	init_task_union.task.state = TASK_RUNNING;
	init_task_union.task.cpu_id = 0;
	// 获取init内核线程的进程控制结构体
	//tmp = container_of(list_next(&task_schedule.task_queue.list),struct task_struct,list);

	//switch_to(current,tmp);
}

