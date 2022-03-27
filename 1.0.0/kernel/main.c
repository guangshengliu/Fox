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

/*
		static var 
		将这些变量放到kernel.lds链接脚本指定的地址处
*/

struct KERNEL_BOOT_PARAMETER_INFORMATION *boot_para_info = (struct KERNEL_BOOT_PARAMETER_INFORMATION *)0xffff800000060000;

void Start_Kernel(void)
{
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

	load_TR(10);

	set_tss64(_stack_start, _stack_start, _stack_start, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00);

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
	
	color_printk(RED,BLACK,"interrupt init \n");

	#if  APIC
		APIC_IOAPIC_init();
	#else
		init_8259A();
	#endif
	
	while(1)
		;
}
