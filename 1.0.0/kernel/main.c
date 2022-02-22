#include "UEFI_boot_param_info.h"
#include "lib.h"
#include "printk.h"
#include "gate.h"
#include "trap.h"
#include "memory.h"
#include "interrupt.h"
#include "task.h"

/*
		static var 
		将这些变量放到kernel.lds链接脚本指定的地址处
*/

struct KERNEL_BOOT_PARAMETER_INFORMATION *boot_para_info = (struct KERNEL_BOOT_PARAMETER_INFORMATION *)0xffff800000060000;

void Start_Kernel(void)
{
	int *addr = (int *)0xffff800003000000;
	int i;
	memset((void *)&_bss, 0, (unsigned long)&_ebss - (unsigned long)&_bss);

	Pos.XResolution = boot_para_info->Graphics_Info.HorizontalResolution;
	Pos.YResolution = boot_para_info->Graphics_Info.VerticalResolution;

	Pos.FB_addr = (int *)0xffff800003000000;
	Pos.FB_length = (Pos.XResolution * Pos.YResolution * 4 + PAGE_4K_SIZE - 1) & PAGE_4K_MASK;

	load_TR(10);

	set_tss64(_stack_start, _stack_start, _stack_start, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00);

	sys_vector_init();

	memory_management_struct.start_code = (unsigned long)& _text;
	memory_management_struct.end_code   = (unsigned long)& _etext;
	memory_management_struct.end_data   = (unsigned long)& _edata;
	memory_management_struct.end_brk    = (unsigned long)& _end;

	//color_printk(RED,BLACK,"boot_para_info->Graphics_Info.HorizontalResolution:%#018lx\tboot_para_info->Graphics_Info.VerticalResolution:%#018lx\tboot_para_info->Graphics_Info.PixelsPerScanLine:%#018lx\n",boot_para_info->Graphics_Info.HorizontalResolution,boot_para_info->Graphics_Info.VerticalResolution,boot_para_info->Graphics_Info.PixelsPerScanLine);
	//color_printk(RED,BLACK,"boot_para_info->Graphics_Info.FrameBufferBase:%#018lx\tboot_para_info->Graphics_Info.FrameBufferSize:%#018lx\n",boot_para_info->Graphics_Info.FrameBufferBase,boot_para_info->Graphics_Info.FrameBufferSize);

	// 抛出除法错误
	// i = 1/0;

	// 页错误
	//i = *(int *)0xffff80000aa00000;

	//color_printk(RED,BLACK,"memory init \n");
	init_memory();

	/*	分配64页内存
	color_printk(RED,BLACK,"memory_management_struct.bits_map:%#018lx\n",*memory_management_struct.bits_map);
	color_printk(RED,BLACK,"memory_management_struct.bits_map:%#018lx\n",*(memory_management_struct.bits_map + 1));

	page = alloc_pages(ZONE_NORMAL,64,PG_PTable_Maped | PG_Active | PG_Kernel);

	for(i = 0;i <= 64;i++)
	{
		color_printk(INDIGO,BLACK,"page%d\tattribute:%#018lx\taddress:%#018lx\t",i,(page + i)->attribute,(page + i)->PHY_address);
		i++;
		color_printk(INDIGO,BLACK,"page%d\tattribute:%#018lx\taddress:%#018lx\n",i,(page + i)->attribute,(page + i)->PHY_address);
	}

	color_printk(RED,BLACK,"memory_management_struct.bits_map:%#018lx\n",*memory_management_struct.bits_map);
	color_printk(RED,BLACK,"memory_management_struct.bits_map:%#018lx\n",*(memory_management_struct.bits_map + 1));
	*/

	//color_printk(RED,BLACK,"interrupt init \n");
	init_interrupt();

	//color_printk(RED,BLACK,"task_init \n");
	task_init();

	while(1);
}
