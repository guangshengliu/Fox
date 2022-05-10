#include "UEFI_boot_param_info.h"
#include "lib.h"
#include "printk.h"
#include "gate.h"
#include "trap.h"
#include "memory.h"
#include "interrupt.h"
#include "task.h"
#include "cpu.h"

#if  APIC
#include "APIC.h"
#else
#include "8259A.h"
#endif

#include "keyboard.h"
#include "spinlock.h"
#include "APIC.h"
#include "SMP.h"

/*
		static var 
		将这些变量放到kernel.lds链接脚本指定的地址处
*/

struct KERNEL_BOOT_PARAMETER_INFORMATION *boot_para_info = (struct KERNEL_BOOT_PARAMETER_INFORMATION *)0xffff800000060000;

struct Global_Memory_Descriptor memory_management_struct = {{0},0};

int global_i = 0;

extern spinlock_T SMP_lock;

void Start_Kernel(void)
{
	struct INT_CMD_REG icr_entry;
	unsigned int * tss = NULL;

	int *addr = (int *)0xffff800003000000;
	int i;
	struct Page * page = NULL;
	void * tmp = NULL;
	struct Slab *slab = NULL;
	
	memset((void *)&_bss, 0, (unsigned long)&_ebss - (unsigned long)&_bss);
	// 此处大坑，分辨率为1366时不能被4整除，需要和scanline保持一致
	// 行业规范，具体原因不清楚
	if((boot_para_info->Graphics_Info.HorizontalResolution) % 4 != 0)
		Pos.XResolution = boot_para_info->Graphics_Info.PixelsPerScanLine;
	else
		Pos.XResolution = boot_para_info->Graphics_Info.HorizontalResolution;
	Pos.YResolution = boot_para_info->Graphics_Info.VerticalResolution;

	Pos.FB_addr = (int *)0xffff800003000000;
	Pos.FB_length = (Pos.XResolution * Pos.YResolution * 4 + PAGE_4K_SIZE - 1) & PAGE_4K_MASK;

	spin_init(&Pos.printk_lock);
	load_TR(10);

	set_tss64(TSS64_Table,_stack_start, _stack_start, _stack_start, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00);
	sys_vector_init();
	
	//init_cpu();

	memory_management_struct.start_code = (unsigned long)& _text;
	memory_management_struct.end_code   = (unsigned long)& _etext;
	memory_management_struct.end_data   = (unsigned long)& _edata;
	memory_management_struct.end_brk    = (unsigned long)& _end;

	//color_printk(RED,BLACK,"boot_para_info->Graphics_Info.HorizontalResolution:%#018lx\tboot_para_info->Graphics_Info.VerticalResolution:%#018lx\tboot_para_info->Graphics_Info.PixelsPerScanLine:%#018lx\n",boot_para_info->Graphics_Info.HorizontalResolution,boot_para_info->Graphics_Info.VerticalResolution,boot_para_info->Graphics_Info.PixelsPerScanLine);
	//color_printk(RED,BLACK,"boot_para_info->Graphics_Info.FrameBufferBase:%#018lx\tboot_para_info->Graphics_Info.FrameBufferSize:%#018lx\n",boot_para_info->Graphics_Info.FrameBufferBase,boot_para_info->Graphics_Info.FrameBufferSize);

	color_printk(RED,BLACK,"memory init \n");
	init_memory();

	color_printk(RED,BLACK,"slab init \n");
	slab_init();

	color_printk(RED,BLACK,"frame buffer init \n");
	frame_buffer_init();
	color_printk(WHITE,BLACK,"frame_buffer_init() is OK \n");

	color_printk(RED,BLACK,"pagetable init \n");	
	pagetable_init();
		
	Local_APIC_init();

	color_printk(RED,BLACK,"ICR init \n");	

	SMP_init();

	//prepare send INIT IPI
	
	icr_entry.vector = 0x00;
	icr_entry.deliver_mode =  APIC_ICR_IOAPIC_INIT;
	icr_entry.dest_mode = ICR_IOAPIC_DELV_PHYSICAL;
	icr_entry.deliver_status = APIC_ICR_IOAPIC_Idle;
	icr_entry.res_1 = 0;
	icr_entry.level = ICR_LEVEL_DE_ASSERT;
	icr_entry.trigger = APIC_ICR_IOAPIC_Edge;
	icr_entry.res_2 = 0;
	icr_entry.dest_shorthand = ICR_ALL_EXCLUDE_Self;
	icr_entry.res_3 = 0;
	icr_entry.destination.x2apic_destination = 0x00;
	
	wrmsr(0x830,*(unsigned long *)&icr_entry);	//INIT IPI

	//prepare send Start-up IPI

	for(global_i = 1;global_i < 4;global_i++)
	{
		spin_lock(&SMP_lock);

		_stack_start = (unsigned long)kmalloc(STACK_SIZE,0) + STACK_SIZE;
		tss = (unsigned int *)kmalloc(128,0);
		set_tss_descriptor(10 + global_i * 2,tss);
		set_tss64(tss,_stack_start,_stack_start,_stack_start,_stack_start,_stack_start,_stack_start,_stack_start,_stack_start,_stack_start,_stack_start);
	
		icr_entry.vector = 0x20;
		icr_entry.deliver_mode = ICR_Start_up;
		icr_entry.dest_shorthand = ICR_No_Shorthand;
		icr_entry.destination.x2apic_destination = global_i;
	
		wrmsr(0x830,*(unsigned long *)&icr_entry);	//Start-up IPI
		wrmsr(0x830,*(unsigned long *)&icr_entry);	//Start-up IPI
	}

	icr_entry.vector = 0xc8;
	icr_entry.destination.x2apic_destination = 1;
	icr_entry.deliver_mode = APIC_ICR_IOAPIC_Fixed;

	wrmsr(0x830,*(unsigned long *)&icr_entry);

	icr_entry.vector = 0xc9;
	wrmsr(0x830,*(unsigned long *)&icr_entry);

	while(1)
		;
}
