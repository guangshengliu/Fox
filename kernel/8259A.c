#include "interrupt.h"
#include "linkage.h"
#include "lib.h"
#include "printk.h"
#include "memory.h"
#include "gate.h"
#include "ptrace.h"

/*
*	Initialize the master/slave 8259A interrupt controller and the gate descriptors in the interrupt descriptor table
*/
void init_8259A()
{
	int i;
	for(i = 32;i < 56;i++)
	{
		set_intr_gate(i , 2 , interrupt[i - 32]);
	}

	color_printk(RED,BLACK,"8259A init \n");

	//	Master/slave 8259A interrupt controller initialization assignment
	//	8259A-master	ICW1-4
	io_out8(0x20,0x11);
	io_out8(0x21,0x20);
	io_out8(0x21,0x04);
	io_out8(0x21,0x01);

	//8259A-slave	ICW1-4
	io_out8(0xa0,0x11);
	io_out8(0xa1,0x28);
	io_out8(0xa1,0x02);
	io_out8(0xa1,0x01);

	//8259A-M/S	OCW1
	io_out8(0x21,0xfd);
	io_out8(0xa1,0xff);

	sti();
}

/*
*	Show the interrupt vector number of the current interrupt request
*/
void do_IRQ(struct pt_regs * regs,unsigned long nr)	//regs,nr
{
	unsigned char x;
	color_printk(RED,BLACK,"do_IRQ:%#018lx\t",nr);
	//	Read the keyboard scan code from the I/O port address 60H
	x = io_in8(0x60);
	color_printk(RED,BLACK,"key code:%#018lx\t",x);
	io_out8(0x20,0x20);
	color_printk(RED,BLACK,"regs:%#018lx\t<RIP:%#018lx\tRSP:%#018lx>\n",regs,regs->rip,regs->rsp);
}



