#include "HPET.h"
#include "printk.h"
#include "memory.h"
#include "lib.h"
#include "APIC.h"
#include "time.h"
#include "timer.h"
#include "softirq.h"
#include "task.h"
#include "schedule.h"
#include "SMP.h"


hw_int_controller HPET_int_controller = 
{
	.enable = IOAPIC_enable,
	.disable = IOAPIC_disable,
	.install = IOAPIC_install,
	.uninstall = IOAPIC_uninstall,
	.ack = IOAPIC_edge_ack,
};

/*
*	HPET handler function
*/

void HPET_handler(unsigned long nr, unsigned long parameter, struct pt_regs * regs)
{
	struct INT_CMD_REG icr_entry;
	jiffies++;

	memset(&icr_entry,0,sizeof(struct INT_CMD_REG));
	icr_entry.vector = 0xc8;
	icr_entry.dest_shorthand = ICR_ALL_EXCLUDE_Self;
	icr_entry.trigger = APIC_ICR_IOAPIC_Edge;
	icr_entry.dest_mode = ICR_IOAPIC_DELV_PHYSICAL;
	icr_entry.deliver_mode = APIC_ICR_IOAPIC_Fixed;
	wrmsr(0x830,*(unsigned long *)&icr_entry);
	
	if((container_of(list_next(&timer_list_head.list),struct timer_list,list)->expire_jiffies <= jiffies))
		set_softirq_status(TIMER_SIRQ);
	
	switch(current->priority)
	{
		case 0:
		case 1:
			task_schedule[SMP_cpu_id()].CPU_exec_task_jiffies--;
			current->vrun_time += 1;
			break;
		case 2:
		default:
			task_schedule[SMP_cpu_id()].CPU_exec_task_jiffies -= 2;
			current->vrun_time += 2;
			break;
	}

	if(task_schedule[SMP_cpu_id()].CPU_exec_task_jiffies <= 0)
		current->flags |= NEED_SCHEDULE;
}

extern struct time time;

void HPET_init()
{
	unsigned int x;
	unsigned int * p = NULL;
	unsigned char * HPET_addr = (unsigned char *)Phy_To_Virt(0xfed00000);

	struct IO_APIC_RET_entry entry;

	
	//get RCBA address
	io_out32(0xcf8,0x8000f8f0);
	x = io_in32(0xcfc);
	x = x & 0xffffc000;	

	//get HPTC address
	if(x > 0xfec00000 && x < 0xfee00000)
	{
		p = (unsigned int *)Phy_To_Virt(x + 0x3404UL);
	}

	//enable HPET
	*p = 0x80;
	io_mfence();
	//init I/O APIC IRQ2 => HPET Timer 0
	entry.vector = 34;
	entry.deliver_mode = APIC_ICR_IOAPIC_Fixed ;
	entry.dest_mode = ICR_IOAPIC_DELV_PHYSICAL;
	entry.deliver_status = APIC_ICR_IOAPIC_Idle;
	entry.polarity = APIC_IOAPIC_POLARITY_HIGH;
	entry.irr = APIC_IOAPIC_IRR_RESET;
	entry.trigger = APIC_ICR_IOAPIC_Edge;
	entry.mask = APIC_ICR_IOAPIC_Masked;
	entry.reserved = 0;

	entry.destination.physical.reserved1 = 0;
	entry.destination.physical.phy_dest = 0;
	entry.destination.physical.reserved2 = 0;

	register_irq(34, &entry , &HPET_handler, NULL, &HPET_int_controller, "HPET");
	
	//color_printk(RED,BLACK,"HPET - GCAP_ID:<%#018lx>\n",*(unsigned long *)HPET_addr);

	*(unsigned long *)(HPET_addr + 0x10) = 3;
	io_mfence();

	//TIM0_CONF register --> edge triggered & periodic
	*(unsigned long *)(HPET_addr + 0x100) = 0x004c;
	io_mfence();

	//TIM0_COMP register --> 1s
	*(unsigned long *)(HPET_addr + 0x108) = 14318179;
	io_mfence();

	//init MAIN_CNT & get CMOS time
	get_cmos_time(&time);
	*(unsigned long *)(HPET_addr + 0xf0) = 0;
	io_mfence();
	
	//color_printk(RED,BLACK,"year:%#010x,month:%#010x,day:%#010x,hour:%#010x,mintue:%#010x,second:%#010x\n",time.year,time.month,time.day,time.hour,time.minute,time.second);
}
