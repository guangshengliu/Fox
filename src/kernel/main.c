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

extern char _text;
extern char _etext;
extern char _edata;
extern char _end;

struct Global_Memory_Descriptor memory_management_struct = {{0},0};

void Start_Kernel(void)
{
	int *addr = (int *)0xffff800000a00000;
	int i;

	struct Page * page = NULL;
	
	// 屏幕分辨率
	Pos.XResolution = 1440;
	Pos.YResolution = 900;
	// 字符光标所在位置
	Pos.XPosition = 0;
	Pos.YPosition = 0;
	// 字符矩阵的尺寸
	Pos.XCharSize = 8;
	Pos.YCharSize = 16;
	// 帧缓冲区起始线性地址
	Pos.FB_addr = (int *)0xffff800000a00000;
	// 缓冲区长度
	Pos.FB_length = (Pos.XResolution * Pos.YResolution * 4 + PAGE_4K_SIZE - 1) & PAGE_4K_MASK;

	load_TR(10);

	set_tss64(0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00);

	sys_vector_init();

	memory_management_struct.start_code = (unsigned long)& _text;
	memory_management_struct.end_code   = (unsigned long)& _etext;
	memory_management_struct.end_data   = (unsigned long)& _edata;
	memory_management_struct.end_brk    = (unsigned long)& _end;

	// 抛出除法错误
	// i = 1/0;

	// 页错误
	//i = *(int *)0xffff80000aa00000;

	color_printk(RED,BLACK,"memory init \n");
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

	color_printk(RED,BLACK,"interrupt init \n");
	init_interrupt();

	color_printk(RED,BLACK,"task_init \n");
	task_init();

	while(1)
		;
}
