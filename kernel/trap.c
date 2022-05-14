#include "trap.h"
#include "gate.h"
#include "ptrace.h"
#include "printk.h"
#include "SMP.h"

int lookup_kallsyms(unsigned long address,int level)
{
	int index = 0;
	int level_index = 0;
	char * string =(char *) &kallsyms_names;
	for(index = 0;index < kallsyms_syms_num;index++)
		if(address >= kallsyms_addresses[index] && address < kallsyms_addresses[index+1])
			break;
	if(index < kallsyms_syms_num)
	{
		for(level_index = 0;level_index < level;level_index++)
			color_printk(RED,BLACK,"  ");
		color_printk(RED,BLACK,"+---> ");

		color_printk(RED,BLACK,"address:%#018lx \t(+) %04d function:%s\n",address,address - kallsyms_addresses[index],&string[kallsyms_index[index]]);
		return 0;
	}
	else
		return 1;
}

void backtrace(struct pt_regs * regs)
{
	unsigned long *rbp = (unsigned long *)regs->rbp;
	unsigned long ret_address = regs->rip;
	int i = 0;

	color_printk(RED,BLACK,"====================== Task Struct Information =====================\n");
	color_printk(RED,BLACK,"regs->rsp:%#018lx,current->thread->rsp0:%#018lx,current:%#018lx\n",regs->rsp,current->thread->rsp0,current);
	color_printk(RED,BLACK,"====================== Kernel Stack Backtrace ======================\n");

	for(i = 0;i<10;i++)
	{
		if(lookup_kallsyms(ret_address,i))
			break;
		if((unsigned long)rbp < (unsigned long)regs->rsp || (unsigned long)rbp > current->thread->rsp0)
			break;

		ret_address = *(rbp+1);
		rbp = (unsigned long *)*rbp;
	}
}

/*
* 除法错误处理函数(#DE)：错误	DIV或IDIV指令
*/

void do_divide_error(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_divide_error(0),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx,CPU:%#018lx\n",error_code , regs->rsp , regs->rip , SMP_cpu_id());
	while(1);
}

/*
* 调试异常处理函数(#DB)：错误/异常	Intel处理器使用
*/

void do_debug(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_debug(1),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* NMI不可屏蔽中断的处理函数(#BP)：中断	不可屏蔽中断
*/

void do_nmi(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_nmi(2),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 断点异常处理函数(#BP)：陷阱	INT3指令
*/

void do_int3(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_int3(3),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 溢出异常处理函数(#OF)：陷阱	INTO指令
*/

void do_overflow(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_overflow(4),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 越界异常处理函数(#BR)：错误	BOUND指令
*/

void do_bounds(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_bounds(5),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 无效/未定义的机器码处理函数(#UD)：错误	UD2指令或保留的机器码
*/

void do_undefined_opcode(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_undefined_opcode(6),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 设备异常处理函数(#NM)：错误	浮点指令WAIT/FWAIT指令
*/

void do_dev_not_available(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_dev_not_available(7),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 双重错误处理函数(#DF)：终止	任何异常，NMI中断或INTR中断
*/

void do_double_fault(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_double_fault(8),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 协处理器段越界处理函数：错误	浮点指令
*/

void do_coprocessor_segment_overrun(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_coprocessor_segment_overrun(9),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 无效TSS段处理函数(TS)：错误	访问TSS段或任务切换
*/

void do_invalid_TSS(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_invalid_TSS(10),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);

	if(error_code & 0x01)
		color_printk(RED,BLACK,"The exception occurred during delivery of an event external to the program,such as an interrupt or an earlier exception.\n");

	if(error_code & 0x02)
		color_printk(RED,BLACK,"Refers to a gate descriptor in the IDT;\n");
	else
		color_printk(RED,BLACK,"Refers to a descriptor in the GDT or the current LDT;\n");

	if((error_code & 0x02) == 0)
		if(error_code & 0x04)
			color_printk(RED,BLACK,"Refers to a segment or gate descriptor in the LDT;\n");
		else
			color_printk(RED,BLACK,"Refers to a descriptor in the current GDT;\n");

	color_printk(RED,BLACK,"Segment Selector Index:%#010x\n",error_code & 0xfff8);

	while(1);
}

/*
* 段不存在处理函数(#NP)：错误	加载段寄存器或访问系统段
*/

void do_segment_not_present(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_segment_not_present(11),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	if(error_code & 0x01)
		color_printk(RED,BLACK,"The exception occurred during delivery of an event external to the program,such as an interrupt or an earlier exception.\n");

	if(error_code & 0x02)
		color_printk(RED,BLACK,"Refers to a gate descriptor in the IDT;\n");
	else
		color_printk(RED,BLACK,"Refers to a descriptor in the GDT or the current LDT;\n");

	if((error_code & 0x02) == 0)
		if(error_code & 0x04)
			color_printk(RED,BLACK,"Refers to a segment or gate descriptor in the LDT;\n");
		else
			color_printk(RED,BLACK,"Refers to a descriptor in the current GDT;\n");

	color_printk(RED,BLACK,"Segment Selector Index:%#010x\n",error_code & 0xfff8);

	while(1);
}

/*
* SS段错误处理函数(#SS)：错误	栈操作或加载栈段寄存器SS
*/

void do_stack_segment_fault(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_stack_segment_fault(12),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);

	if(error_code & 0x01)
		color_printk(RED,BLACK,"The exception occurred during delivery of an event external to the program,such as an interrupt or an earlier exception.\n");

	if(error_code & 0x02)
		color_printk(RED,BLACK,"Refers to a gate descriptor in the IDT;\n");
	else
		color_printk(RED,BLACK,"Refers to a descriptor in the GDT or the current LDT;\n");

	if((error_code & 0x02) == 0)
		if(error_code & 0x04)
			color_printk(RED,BLACK,"Refers to a segment or gate descriptor in the LDT;\n");
		else
			color_printk(RED,BLACK,"Refers to a descriptor in the current GDT;\n");

	color_printk(RED,BLACK,"Segment Selector Index:%#010x\n",error_code & 0xfff8);

	while(1);
}

/*
* 通用保护性异常处理函数(#do_general_protection)：错误	任何内存引用和保护检测
*/

void do_general_protection(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_general_protection(13),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);

	if(error_code & 0x01)
		color_printk(RED,BLACK,"The exception occurred during delivery of an event external to the program,such as an interrupt or an earlier exception.\n");

	if(error_code & 0x02)
		color_printk(RED,BLACK,"Refers to a gate descriptor in the IDT;\n");
	else
		color_printk(RED,BLACK,"Refers to a descriptor in the GDT or the current LDT;\n");

	if((error_code & 0x02) == 0)
		if(error_code & 0x04)
			color_printk(RED,BLACK,"Refers to a segment or gate descriptor in the LDT;\n");
		else
			color_printk(RED,BLACK,"Refers to a descriptor in the current GDT;\n");

	color_printk(RED,BLACK,"Segment Selector Index:%#010x\n",error_code & 0xfff8);

	while(1);
}

/*
* 页错误处理函数：错误 任何内存引用
*/

void do_page_fault(struct pt_regs * regs,unsigned long error_code)
{
	unsigned long cr2 = 0;
	// 将CR2控制寄存器的值保存到变量cr2里
	__asm__	__volatile__("movq	%%cr2,	%0":"=r"(cr2)::"memory");
	// 将栈指针寄存器RSP的值向上索引0X98个字节
	// 获取被中断程序执行现场的RIP寄存器值，并作为产生异常指令的地址值
	color_printk(RED,BLACK,"do_page_fault(14),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);

	if(!(error_code & 0x01))	// p标志位为0
		color_printk(RED,BLACK,"Page Not-Present,\t");

	if(error_code & 0x02)	// W/R=0
		color_printk(RED,BLACK,"Write Cause Fault,\t");
	else		// W/R=1
		color_printk(RED,BLACK,"Read Cause Fault,\t");

	if(error_code & 0x04)	// U/S=1
		color_printk(RED,BLACK,"Fault in user(3)\t");
	else		// U/S=0
		color_printk(RED,BLACK,"Fault in supervisor(0,1,2)\t");

	if(error_code & 0x08)	// PSVD=1
		color_printk(RED,BLACK,",Reserved Bit Cause Fault\t");

	if(error_code & 0x10)	// I/D=1
		color_printk(RED,BLACK,",Instruction fetch Cause Fault");

	color_printk(RED,BLACK,"\n");
	// CR2控制寄存器保存着触发异常时的线性地址
	color_printk(RED,BLACK,"CR2:%#018lx\n",cr2);
	// 保持死循环
	while(1);
}

/*
* x87FPU错误处理函数(#MF)：错误	x87FPU浮点指令或WAIT/FWAIT指令
*/

void do_x87_FPU_error(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_x87_FPU_error(16),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 对齐检测处理函数(#AC)：错误	引用内存中的任何数据
*/

void do_alignment_check(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_alignment_check(17),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 机器检测处理函数(#MC)：终止	错误码与其CPU类型有关
*/

void do_machine_check(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_machine_check(18),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* SIMD浮点异常处理函数(#XM)：错误	SSE/SSE2/SSE3浮点指令
*/

void do_SIMD_exception(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_SIMD_exception(19),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 虚拟化异常处理函数(#NP)：错误	违反EPT
*/

void do_virtualization_exception(struct pt_regs * regs,unsigned long error_code)
{
	color_printk(RED,BLACK,"do_virtualization_exception(20),ERROR_CODE:%#018lx,RSP:%#018lx,RIP:%#018lx\n",error_code , regs->rsp , regs->rip);
	while(1);
}

/*
* 为各种向量配置了处理函数和栈指针
*/

void sys_vector_init()
{
	set_trap_gate(0,0,divide_error);
	set_trap_gate(1,0,debug);
	set_intr_gate(2,0,nmi);
	set_system_gate(3,0,int3);
	set_system_gate(4,0,overflow);
	set_system_gate(5,0,bounds);
	set_trap_gate(6,0,undefined_opcode);
	set_trap_gate(7,0,dev_not_available);
	set_trap_gate(8,0,double_fault);
	set_trap_gate(9,0,coprocessor_segment_overrun);
	set_trap_gate(10,0,invalid_TSS);
	set_trap_gate(11,0,segment_not_present);
	set_trap_gate(12,0,stack_segment_fault);
	set_trap_gate(13,0,general_protection);
	set_trap_gate(14,0,page_fault);
	//15 Intel reserved. Do not use.
	set_trap_gate(16,0,x87_FPU_error);
	set_trap_gate(17,0,alignment_check);
	set_trap_gate(18,0,machine_check);
	set_trap_gate(19,0,SIMD_exception);
	set_trap_gate(20,0,virtualization_exception);

	//set_system_gate(SYSTEM_CALL_VECTOR,0,system_call);

}

