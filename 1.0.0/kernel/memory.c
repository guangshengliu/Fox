#include "UEFI_boot_param_info.h"
#include "memory.h"
#include "lib.h"



struct Global_Memory_Descriptor memory_management_struct = {{0},0};

/*
*	Init page
*/
unsigned long page_init(struct Page * page,unsigned long flags)
{
	page->attribute |= flags;
	
	if(!page->reference_count || (page->attribute & PG_Shared))
	{
		page->reference_count++;
		page->zone_struct->total_pages_link++;		
	}	
	
	return 1;
}

/*
*	Decrease reference page 
*/
unsigned long page_clean(struct Page * page)
{
	page->reference_count--;
	page->zone_struct->total_pages_link--;

	if(!page->reference_count)
	{
		page->attribute &= PG_PTable_Maped;
	}
	
	return 1;
}

/*
*	Get page attribute
*/
unsigned long get_page_attribute(struct Page * page)
{
	if(page == NULL)
	{
		color_printk(RED,BLACK,"get_page_attribute() ERROR: page == NULL\n");
		return 0;
	}
	else
		return page->attribute;
}

/*
*	Alert page attribute
*/
unsigned long set_page_attribute(struct Page * page,unsigned long flags)
{
	if(page == NULL)
	{
		color_printk(RED,BLACK,"set_page_attribute() ERROR: page == NULL\n");
		return 0;
	}
	else
	{
		page->attribute = flags;
		return 1;
	}
}

void init_memory()
{
	int i,j;
	unsigned long TotalMem = 0 ;
	struct EFI_E820_MEMORY_DESCRIPTOR *p = NULL;	

	color_printk(BLUE,BLACK,"Display Physics Address MAP,Type(1:RAM,2:ROM or Reserved,3:ACPI Reclaim Memory,4:ACPI NVS Memory,Others:Undefine)\n");
	p = (struct EFI_E820_MEMORY_DESCRIPTOR *)boot_para_info->E820_Info.E820_Entry;

	for(i = 0;i < boot_para_info->E820_Info.E820_Entry_count;i++)
	{
		color_printk(GREEN,BLACK,"UEFI=>KERN---Address:%#018lx\tLength:%#018lx\tType:%#010x\n",p->address,p->length,p->type);
		if(p->type == 1)
			TotalMem +=  p->length;

		memory_management_struct.e820[i].address += p->address;
		memory_management_struct.e820[i].length	 += p->length;
		memory_management_struct.e820[i].type	 = p->type;
		memory_management_struct.e820_length = i;

		p++;
		// 出现type>4的情况，则是遇见程序运行时产生的脏数据
		// p->length==0||p->type<1，来截断并剔除E820数组中的脏数据
		if(p->type > 4 || p->length == 0 || p->type < 1)
			break;		
	}

	color_printk(ORANGE,BLACK,"OS Can Used Total RAM:%#018lx\n",TotalMem);

	TotalMem = 0;

	for(i = 0;i <= memory_management_struct.e820_length;i++)
	{
		unsigned long start,end;
		if(memory_management_struct.e820[i].type != 1)
			continue;
		// 起始地址按2MB页的上边界对齐
		start = PAGE_2M_ALIGN(memory_management_struct.e820[i].address);
		// 下边界对齐操作
		end   = ((memory_management_struct.e820[i].address + memory_management_struct.e820[i].length) >> PAGE_2M_SHIFT) << PAGE_2M_SHIFT;
		if(end <= start)
			continue;
		// TotalMem += (end - start) / (2^21)
		TotalMem += (end - start) >> PAGE_2M_SHIFT;
	}
	
	color_printk(ORANGE,BLACK,"OS Can Used Total 2M PAGEs:%#010x=%010d\n",TotalMem,TotalMem);
	// 计算出物理空间总大小
	TotalMem = memory_management_struct.e820[memory_management_struct.e820_length].address + memory_management_struct.e820[memory_management_struct.e820_length].length;

	//bits map construction init
	// 映射位图指针
	memory_management_struct.bits_map = (unsigned long *)((memory_management_struct.end_brk + PAGE_4K_SIZE - 1) & PAGE_4K_MASK);
	// 物理空间可分页数
	memory_management_struct.bits_size = TotalMem >> PAGE_2M_SHIFT;
	// 物理空间可映射位图长度
	memory_management_struct.bits_length = (((unsigned long)(TotalMem >> PAGE_2M_SHIFT) + sizeof(long) * 8 - 1) / 8) & ( ~ (sizeof(long) - 1));

	memset(memory_management_struct.bits_map,0xff,memory_management_struct.bits_length);		//init bits map memory

	//pages construction init
	// 结构体数组
	memory_management_struct.pages_struct = (struct Page *)(((unsigned long)memory_management_struct.bits_map + memory_management_struct.bits_length + PAGE_4K_SIZE - 1) & PAGE_4K_MASK);
	// 物理空间可分页数
	memory_management_struct.pages_size = TotalMem >> PAGE_2M_SHIFT;
	// 物理空间结构体数组长度
	memory_management_struct.pages_length = ((TotalMem >> PAGE_2M_SHIFT) * sizeof(struct Page) + sizeof(long) - 1) & ( ~ (sizeof(long) - 1));

	memset(memory_management_struct.pages_struct,0x00,memory_management_struct.pages_length);	//init pages memory

	//zones construction init
	// 结构体数组
	memory_management_struct.zones_struct = (struct Zone *)(((unsigned long)memory_management_struct.pages_struct + memory_management_struct.pages_length + PAGE_4K_SIZE - 1) & PAGE_4K_MASK);
	// 暂时定义为0
	memory_management_struct.zones_size   = 0;
	// 暂时按照5个struct zone结构体计算
	memory_management_struct.zones_length = (5 * sizeof(struct Zone) + sizeof(long) - 1) & (~(sizeof(long) - 1));

	memset(memory_management_struct.zones_struct,0x00,memory_management_struct.zones_length);	//init zones memory



	for(i = 0;i <= memory_management_struct.e820_length;i++)
	{
		unsigned long start,end;
		struct Zone * z;
		struct Page * p;
		unsigned long * b;

		// 过滤掉非物理内存段
		if(memory_management_struct.e820[i].type != 1)
			continue;
		// 可用物理内存段进行页对齐
		start = PAGE_2M_ALIGN(memory_management_struct.e820[i].address);
		end   = ((memory_management_struct.e820[i].address + memory_management_struct.e820[i].length) >> PAGE_2M_SHIFT) << PAGE_2M_SHIFT;
		if(end <= start)
			continue;
		
		//zone init

		z = memory_management_struct.zones_struct + memory_management_struct.zones_size;
		memory_management_struct.zones_size++;

		z->zone_start_address = start;
		z->zone_end_address = end;
		z->zone_length = end - start;

		z->page_using_count = 0;
		z->page_free_count = (end - start) >> PAGE_2M_SHIFT;

		z->total_pages_link = 0;

		z->attribute = 0;
		z->GMD_struct = &memory_management_struct;

		z->pages_length = (end - start) >> PAGE_2M_SHIFT;
		z->pages_group =  (struct Page *)(memory_management_struct.pages_struct + (start >> PAGE_2M_SHIFT));

		//page init
		p = z->pages_group;
		for(j = 0;j < z->pages_length; j++ , p++)
		{
			p->zone_struct = z;
			p->PHY_address = start + PAGE_2M_SIZE * j;
			p->attribute = 0;

			p->reference_count = 0;

			p->age = 0;
			// 将当前struct page结构体所代表的物理地址转换成bits_map映射位图中对应的位
			*(memory_management_struct.bits_map + ((p->PHY_address >> PAGE_2M_SHIFT) >> 6)) ^= 1UL << (p->PHY_address >> PAGE_2M_SHIFT) % 64;

		}
		
	}

	/////////////init address 0 to page struct 0; because the memory_management_struct.e820[0].type != 1
	
	memory_management_struct.pages_struct->zone_struct = memory_management_struct.zones_struct;

	memory_management_struct.pages_struct->PHY_address = 0UL;

	set_page_attribute(memory_management_struct.pages_struct,PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
	memory_management_struct.pages_struct->reference_count = 1;
	memory_management_struct.pages_struct->age = 0;

	/////////////
	// 	计算出struct zone区域空间结构体数组的元素数量
	memory_management_struct.zones_length = (memory_management_struct.zones_size * sizeof(struct Zone) + sizeof(long) - 1) & ( ~ (sizeof(long) - 1));

	color_printk(ORANGE,BLACK,"bits_map:%#018lx,bits_size:%#018lx,bits_length:%#018lx\n",memory_management_struct.bits_map,memory_management_struct.bits_size,memory_management_struct.bits_length);

	color_printk(ORANGE,BLACK,"pages_struct:%#018lx,pages_size:%#018lx,pages_length:%#018lx\n",memory_management_struct.pages_struct,memory_management_struct.pages_size,memory_management_struct.pages_length);

	color_printk(ORANGE,BLACK,"zones_struct:%#018lx,zones_size:%#018lx,zones_length:%#018lx\n",memory_management_struct.zones_struct,memory_management_struct.zones_size,memory_management_struct.zones_length);

	ZONE_DMA_INDEX = 0;	//need rewrite in the future
	ZONE_NORMAL_INDEX = 0;	//need rewrite in the future	
	ZONE_UNMAPED_INDEX = 0;
	
	for(i = 0;i < memory_management_struct.zones_size;i++)	//need rewrite in the future
	{
		struct Zone * z = memory_management_struct.zones_struct + i;
		color_printk(ORANGE,BLACK,"zone_start_address:%#018lx,zone_end_address:%#018lx,zone_length:%#018lx,pages_group:%#018lx,pages_length:%#018lx\n",z->zone_start_address,z->zone_end_address,z->zone_length,z->pages_group,z->pages_length);
		// 当前区域的起始地址是ox100000000，将该区域索引值记录下来
		// 表示该区域空间开始的物理内存未曾经过页表映射
		if(z->zone_start_address >= 0x100000000 && !ZONE_UNMAPED_INDEX)
			ZONE_UNMAPED_INDEX = i;
	}
	
	color_printk(ORANGE,BLACK,"ZONE_DMA_INDEX:%d\tZONE_NORMAL_INDEX:%d\tZONE_UNMAPED_INDEX:%d\n",ZONE_DMA_INDEX,ZONE_NORMAL_INDEX,ZONE_UNMAPED_INDEX);

	memory_management_struct.end_of_struct = (unsigned long)((unsigned long)memory_management_struct.zones_struct + memory_management_struct.zones_length + sizeof(long) * 32) & ( ~ (sizeof(long) - 1));	////need a blank to separate memory_management_struct

	color_printk(ORANGE,BLACK,"start_code:%#018lx,end_code:%#018lx,end_data:%#018lx,start_brk:%#018lx,end_of_struct:%#018lx\n",memory_management_struct.start_code,memory_management_struct.end_code,memory_management_struct.end_data,memory_management_struct.end_brk, memory_management_struct.end_of_struct);
	
	i = Virt_To_Phy(memory_management_struct.end_of_struct) >> PAGE_2M_SHIFT;
	// 将系统内核与内存管理单元结构所占物理页的page结构体全部初始化
	// PG_PTable_Maped 经过页表映射的页 PG_Kernel_Init 内核初始化程序
	// PG_Active 使用中的页 PG_Kernel 内核层页
	for(j = 1;j <= i;j++)
	{
		struct Page * tmp_page =  memory_management_struct.pages_struct + j;
		page_init(tmp_page,PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
		*(memory_management_struct.bits_map + ((tmp_page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |= 1UL << (tmp_page->PHY_address >> PAGE_2M_SHIFT) % 64;
		tmp_page->zone_struct->page_using_count++;
		tmp_page->zone_struct->page_free_count--;
	}

	Global_CR3 = Get_gdt();

	color_printk(INDIGO,BLACK,"Global_CR3\t:%#018lx\n",Global_CR3);
	color_printk(INDIGO,BLACK,"*Global_CR3\t:%#018lx\n",*Phy_To_Virt(Global_CR3) & (~0xff));
	color_printk(PURPLE,BLACK,"**Global_CR3\t:%#018lx\n",*Phy_To_Virt(*Phy_To_Virt(Global_CR3) & (~0xff)) & (~0xff));

	color_printk(ORANGE,BLACK,"1.memory_management_struct.bits_map:%#018lx\tzone_struct->page_using_count:%d\tzone_struct->page_free_count:%d\n",*memory_management_struct.bits_map,memory_management_struct.zones_struct->page_using_count,memory_management_struct.zones_struct->page_free_count);

	for(i = 0;i < 10;i++)
		*(Phy_To_Virt(Global_CR3)  + i) = 0UL;

	flush_tlb();
}

/*

	number: number < 64

	zone_select: zone select from DMA,mapped in pagetable,unmapped in pagetable

	page_flags: struct Page flages

*/

struct Page * alloc_pages(int zone_select,int number,unsigned long page_flags)
{
	int i;
	unsigned long page = 0;
	unsigned long attribute = 0;

	int zone_start = 0;
	int zone_end = 0;

	if(number >= 64 || number <= 0)
	{
		color_printk(RED,BLACK,"alloc_pages() ERROR: number is invalid\n");
		return NULL;		
	}

	//	According to zone_ select determines which the memory to be search
	switch(zone_select)
	{
		case ZONE_DMA:
				zone_start = 0;
				zone_end = ZONE_DMA_INDEX;
				attribute = PG_PTable_Maped;
			break;

		case ZONE_NORMAL:
				zone_start = ZONE_DMA_INDEX;
				zone_end = ZONE_NORMAL_INDEX;
				attribute = PG_PTable_Maped;
			break;

		case ZONE_UNMAPED:
				zone_start = ZONE_UNMAPED_INDEX;
				zone_end = memory_management_struct.zones_size - 1;
				attribute = 0;
			break;

		default:
			color_printk(RED,BLACK,"alloc_pages() ERROR: zone_select index is invalid\n");
			return NULL;
			break;
	}

	//	Start from the beginning of the zone_start and traverse one by one until the end of the zone_end
	for(i = zone_start;i <= zone_end; i++)
	{
		struct Zone * z;
		unsigned long j;
		unsigned long start,end;
		unsigned long tmp;

		if((memory_management_struct.zones_struct + i)->page_free_count < number)
			continue;

		z = memory_management_struct.zones_struct + i;
		start = z->zone_start_address >> PAGE_2M_SHIFT;
		end = z->zone_end_address >> PAGE_2M_SHIFT;

		//	Start % 64 : get zone start address bits_map index
		//	Maybe need 2 bits_map point
		//	For example : first from 23-63 bit and second from 0-22 bit which total need 64 pages
		tmp = 64 - start % 64;
		//	if j == 0 ==> j = 0  else j = start and j = tmp
		for(j = start;j < end;j += j % 64 ? tmp : 64)
		{
			unsigned long * p = memory_management_struct.bits_map + (j >> 6);
			unsigned long k = 0;
			unsigned long shift = j % 64;
			//	number = 2 : num = 0b00000000000000000000000011
			//	number = 3 : num = 0b00000000000000000000000111
			unsigned long num = (1UL << number) - 1;
			
			for(k = shift;k < 64;k++)
			{
				//	*(p+1) << (64-k) splice *p >> k weather equal to num ?
				if( !( (k ? ((*p >> k) | (*(p + 1) << (64 - k))) : *p) & (num) ) )
				{
					unsigned long	l;
					page = j + k - shift;
					for(l = 0;l < number;l++)
					{
						struct Page * pageptr = memory_management_struct.pages_struct + page + l;
						//	Set bits_map[index]
						*(memory_management_struct.bits_map + ((pageptr->PHY_address >> PAGE_2M_SHIFT) >> 6)) |= 1UL << (pageptr->PHY_address >> PAGE_2M_SHIFT) % 64;
						z->page_using_count++;
						z->page_free_count--;
						pageptr->attribute = attribute;
					}
					goto find_free_pages;
				}
			}
		}
	}

	color_printk(RED,BLACK,"alloc_pages() ERROR: no page can alloc\n");
	return NULL;

find_free_pages:

	return (struct Page *)(memory_management_struct.pages_struct + page);
}

/*

	page: free page start from this pointer
	number: number < 64

*/

void free_pages(struct Page * page,int number)
{	
	int i = 0;
	
	if(page == NULL)
	{
		color_printk(RED,BLACK,"free_pages() ERROR: page is invalid\n");
		return ;
	}	

	if(number >= 64 || number <= 0)
	{
		color_printk(RED,BLACK,"free_pages() ERROR: number is invalid\n");
		return ;	
	}
	
	for(i = 0;i<number;i++,page++)
	{
		//	Reset bits_map[index]
		*(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) &= ~(1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64);
		page->zone_struct->page_using_count--;
		page->zone_struct->page_free_count++;
		page->attribute = 0;
	}
}

/*
*	Use kmalloc to select slab memory pool and find slab object from slab memory pool
*	gfp_flages: the condition of get memory 
*/

void * kmalloc(unsigned long size,unsigned long gfp_flages)
{
	int i,j;
	struct Slab * slab = NULL;
	//	Max malloc size : 1MB
	if(size > 1048576)
	{
		color_printk(RED,BLACK,"kmalloc() ERROR: kmalloc size too long:%08d\n",size);
		return NULL;
	}
	//	Select slab memory pool size
	for(i = 0;i < 16;i++)
		if(kmalloc_cache_size[i].size >= size)
			break;
	slab = kmalloc_cache_size[i].cache_pool;

	if(kmalloc_cache_size[i].total_free != 0)
	{
		do
		{
			if(slab->free_count == 0)
				// get SLAB List point
				slab = container_of(list_next(&slab->list),struct Slab,list);
			else
				break;
		}while(slab != kmalloc_cache_size[i].cache_pool);
	}
	else
	{
		slab = kmalloc_create(kmalloc_cache_size[i].size);
		
		if(slab == NULL)
		{
			color_printk(BLUE,BLACK,"kmalloc()->kmalloc_create()=>slab == NULL\n");
			return NULL;
		}
		
		kmalloc_cache_size[i].total_free += slab->color_count;

		color_printk(BLUE,BLACK,"kmalloc()->kmalloc_create()<=size:%#010x\n",kmalloc_cache_size[i].size);
		
		list_add_to_before(&kmalloc_cache_size[i].cache_pool->list,&slab->list);
	}

	for(j = 0;j < slab->color_count;j++)
	{
		if(*(slab->color_map + (j >> 6)) == 0xffffffffffffffffUL)
		{
			//	int j = 63	j = 0b0,0000000000000000000000000111111
			//	int j = 126 j = 0b0,0000000000000000000000001111110
			j += 63;
			continue;
		}
		//	int j = 63	1UL << (j % 64) : 0x8000000000000000UL
		//	int j = 126	1UL << (j % 64) : 0x4000000000000000UL
		//	Whether the int(i/64) bit in the color_map is 0 ? (if bit == 0 means free memory)
		if( (*(slab->color_map + (j >> 6)) & (1UL << (j % 64))) == 0 )
		{
			// Make the int(i/64) bit to 1 (Set the status to using)
			*(slab->color_map + (j >> 6)) |= 1UL << (j % 64);
			slab->using_count++;
			slab->free_count--;

			kmalloc_cache_size[i].total_free--;
			kmalloc_cache_size[i].total_using++;

			return (void *)((char *)slab->Vaddress + kmalloc_cache_size[i].size * j);
		}
	}

	color_printk(BLUE,BLACK,"kmalloc() ERROR: no memory can alloc\n");
	return NULL;
}

/*
*	Use kmalloc_create to create struct Slab
*	parameter size: unsigned long type number
*/

struct Slab * kmalloc_create(unsigned long size)
{
	int i;
	struct Slab * slab = NULL;
	struct Page * page = NULL;
	unsigned long * vaddresss = NULL;
	long structsize = 0;
	// Allocate a physical page
	page = alloc_pages(ZONE_NORMAL,1, 0);
	
	if(page == NULL)
	{
		color_printk(RED,BLACK,"kmalloc_create()->alloc_pages()=>page == NULL\n");
		return NULL;
	}
	
	page_init(page,PG_Kernel);

	switch(size)
	{
		case 32:
		case 64:
		case 128:
		case 256:
		case 512:

			vaddresss = Phy_To_Virt(page->PHY_address);
			//	Recoad the memory size of struct Slab and color_map
			structsize = sizeof(struct Slab) + PAGE_2M_SIZE / size / 8;
			//	Save struct Slab and color_map in the end of the page
			slab = (struct Slab *)((unsigned char *)vaddresss + PAGE_2M_SIZE - structsize);
			//	Save color_map after struct Slab
			slab->color_map = (unsigned long *)((unsigned char *)slab + sizeof(struct Slab));

			slab->free_count = (PAGE_2M_SIZE - (PAGE_2M_SIZE / size / 8) - sizeof(struct Slab)) / size;
			slab->using_count = 0;
			slab->color_count = slab->free_count;
			slab->Vaddress = vaddresss;
			slab->page = page;
			list_init(&slab->list);

			slab->color_length = ((slab->color_count + sizeof(unsigned long) * 8 - 1) >> 6) << 3;
			memset(slab->color_map,0xff,slab->color_length);
			// Init color_map
			for(i = 0;i < slab->color_count;i++)
				*(slab->color_map + (i >> 6)) ^= 1UL << i % 64;

			break;

		//	Kmalloc slab and map,not in 2M page anymore

		case 1024:		//1KB
		case 2048:
		case 4096:		//4KB
		case 8192:
		case 16384:

		//	Color_map is a very short buffer.
		//	Use kmalloc to create a struct Slab.
		//	Different:keep struct Slab and color_map in different page

		case 32768:
		case 65536:
		case 131072:		//128KB
		case 262144:
		case 524288:
		case 1048576:		//1MB

			slab = (struct Slab *)kmalloc(sizeof(struct Slab),0);

			slab->free_count = PAGE_2M_SIZE / size;
			slab->using_count = 0;
			slab->color_count = slab->free_count;

			slab->color_length = ((slab->color_count + sizeof(unsigned long) * 8 - 1) >> 6) << 3;

			slab->color_map = (unsigned long *)kmalloc(slab->color_length,0);
			memset(slab->color_map,0xff,slab->color_length);

			slab->Vaddress = Phy_To_Virt(page->PHY_address);
			slab->page = page;
			list_init(&slab->list);

			for(i = 0;i < slab->color_count;i++)
				*(slab->color_map + (i >> 6)) ^= 1UL << i % 64;

			break;
		//	Find Error
		default:

			color_printk(RED,BLACK,"kmalloc_create() ERROR: wrong size:%08d\n",size);
			free_pages(page,1);
			
			return NULL;
	}	
	
	return slab;
}

/*
*	Free struct Slab
*	Parameter void *address : start address of struct Slab
*/

unsigned long kfree(void * address)
{
	int i;
	int index;
	struct Slab * slab = NULL;
	//	Get the starting vertical address of the current physical page
	void * page_base_address = (void *)((unsigned long)address & PAGE_2M_MASK);
	//	Find page from slab memory pool through page_base_address
	for(i = 0;i < 16;i++)
	{
		slab = kmalloc_cache_size[i].cache_pool;
		do
		{
			if(slab->Vaddress == page_base_address)
			{
				// Reset the color_map index bit
				index = (address - slab->Vaddress) / kmalloc_cache_size[i].size;
				*(slab->color_map + (index >> 6)) ^= 1UL << index % 64;

				slab->free_count++;
				slab->using_count--;

				kmalloc_cache_size[i].total_free++;
				kmalloc_cache_size[i].total_using--;
				//	The number of free objects in the memory pool exceeds 1.5 of the using objects，free this struct Slab
				if((slab->using_count == 0) && (kmalloc_cache_size[i].total_free >= slab->color_count * 3 / 2) && (kmalloc_cache_size[i].cache_pool != slab))
				{
					switch(kmalloc_cache_size[i].size)
					{
						//	slab + map in 2M page
				
						case 32:
						case 64:
						case 128:
						case 256:	
						case 512:
							list_del(&slab->list);
							kmalloc_cache_size[i].total_free -= slab->color_count;

							page_clean(slab->page);
							free_pages(slab->page,1);
							break;
						//	512 < size < 1024
						default:
							list_del(&slab->list);
							kmalloc_cache_size[i].total_free -= slab->color_count;

							kfree(slab->color_map);

							page_clean(slab->page);
							free_pages(slab->page,1);
							kfree(slab);
							break;
					}
 
				}

				return 1;
			}
			else
				slab = container_of(list_next(&slab->list),struct Slab,list);				

		}while(slab != kmalloc_cache_size[i].cache_pool);
	
	}
	
	color_printk(RED,BLACK,"kfree() ERROR: can`t free memory\n");
	
	return 0;
}

/*
*	Create slab momory pool
*/

struct Slab_cache * slab_create(unsigned long size,void *(* constructor)(void * Vaddress,unsigned long arg),void *(* destructor)(void * Vaddress,unsigned long arg),unsigned long arg)
{
	struct Slab_cache * slab_cache = NULL;

	slab_cache = (struct Slab_cache *)kmalloc(sizeof(struct Slab_cache),0);
	
	if(slab_cache == NULL)
	{
		color_printk(RED,BLACK,"slab_create()->kmalloc()=>slab_cache == NULL\n");
		return NULL;
	}
	
	memset(slab_cache,0,sizeof(struct Slab_cache));

	slab_cache->size = SIZEOF_LONG_ALIGN(size);
	slab_cache->total_using = 0;
	slab_cache->total_free = 0;
	slab_cache->cache_pool = (struct Slab *)kmalloc(sizeof(struct Slab),0);
	
	if(slab_cache->cache_pool == NULL)
	{
		color_printk(RED,BLACK,"slab_create()->kmalloc()=>slab_cache->cache_pool == NULL\n");
		kfree(slab_cache);
		return NULL;
	}
	
	memset(slab_cache->cache_pool,0,sizeof(struct Slab));

	slab_cache->cache_dma_pool = NULL;
	slab_cache->constructor = constructor;
	slab_cache->destructor = destructor;

	list_init(&slab_cache->cache_pool->list);

	slab_cache->cache_pool->page = alloc_pages(ZONE_NORMAL,1,0);
	
	if(slab_cache->cache_pool->page == NULL)
	{
		color_printk(RED,BLACK,"slab_create()->alloc_pages()=>slab_cache->cache_pool->page == NULL\n");
		kfree(slab_cache->cache_pool);
		kfree(slab_cache);
		return NULL;
	}
	
	page_init(slab_cache->cache_pool->page,PG_Kernel);

	slab_cache->cache_pool->using_count = PAGE_2M_SIZE/slab_cache->size;

	slab_cache->cache_pool->free_count = slab_cache->cache_pool->using_count;

	slab_cache->total_free = slab_cache->cache_pool->free_count;

	slab_cache->cache_pool->Vaddress = Phy_To_Virt(slab_cache->cache_pool->page->PHY_address);

	slab_cache->cache_pool->color_count = slab_cache->cache_pool->free_count;
	//	sizeof(unsigned long) = 8
	slab_cache->cache_pool->color_length = ((slab_cache->cache_pool->color_count + sizeof(unsigned long) * 8 - 1) >> 6) << 3;

	slab_cache->cache_pool->color_map = (unsigned long *)kmalloc(slab_cache->cache_pool->color_length,0);

	if(slab_cache->cache_pool->color_map == NULL)
	{
		color_printk(RED,BLACK,"slab_create()->kmalloc()=>slab_cache->cache_pool->color_map == NULL\n");
		
		free_pages(slab_cache->cache_pool->page,1);
		kfree(slab_cache->cache_pool);
		kfree(slab_cache);
		return NULL;
	}
	
	memset(slab_cache->cache_pool->color_map,0,slab_cache->cache_pool->color_length);

	return slab_cache;
}

/*
*	Destroy slab memory pool
*/

unsigned long slab_destroy(struct Slab_cache * slab_cache)
{
	struct Slab * slab_p = slab_cache->cache_pool;
	struct Slab * tmp_slab = NULL;

	if(slab_cache->total_using != 0)
	{
		color_printk(RED,BLACK,"slab_cache->total_using != 0\n");
		return 0;
	}

	while(!list_is_empty(&slab_p->list))
	{
		tmp_slab = slab_p;
		slab_p = container_of(list_next(&slab_p->list),struct Slab,list);

		list_del(&tmp_slab->list);
		kfree(tmp_slab->color_map);

		page_clean(tmp_slab->page);
		free_pages(tmp_slab->page,1);
		kfree(tmp_slab);
	}

	kfree(slab_p->color_map);

	page_clean(slab_p->page);
	free_pages(slab_p->page,1);
	kfree(slab_p);
		
	kfree(slab_cache);

	return 1;
}

/*
*	Through slab memory pool to malloc Slab object
*/

void * slab_malloc(struct Slab_cache * slab_cache,unsigned long arg)
{
	struct Slab * slab_p = slab_cache->cache_pool;
	struct Slab * tmp_slab = NULL;
	int j = 0;
	//	No free struct Slab,need malloc more struct Slab
	if(slab_cache->total_free == 0)
	{
		// Malloc memory for struct Slab
		tmp_slab = (struct Slab *)kmalloc(sizeof(struct Slab),0);
	
		if(tmp_slab == NULL)
		{
			color_printk(RED,BLACK,"slab_malloc()->kmalloc()=>tmp_slab == NULL\n");
			return NULL;
		}
	
		memset(tmp_slab,0,sizeof(struct Slab));

		list_init(&tmp_slab->list);
		// Slab manage 1 page
		tmp_slab->page = alloc_pages(ZONE_NORMAL,1,0);
	
		if(tmp_slab->page == NULL)
		{
			color_printk(RED,BLACK,"slab_malloc()->alloc_pages()=>tmp_slab->page == NULL\n");
			kfree(tmp_slab);
			return NULL;
		}
	
		page_init(tmp_slab->page,PG_Kernel);

		tmp_slab->using_count = PAGE_2M_SIZE/slab_cache->size;
		tmp_slab->free_count = tmp_slab->using_count;
		tmp_slab->Vaddress = Phy_To_Virt(tmp_slab->page->PHY_address);

		tmp_slab->color_count = tmp_slab->free_count;
		tmp_slab->color_length = ((tmp_slab->color_count + sizeof(unsigned long) * 8 - 1) >> 6) << 3;
		tmp_slab->color_map = (unsigned long *)kmalloc(tmp_slab->color_length,0);

		if(tmp_slab->color_map == NULL)
		{
			color_printk(RED,BLACK,"slab_malloc()->kmalloc()=>tmp_slab->color_map == NULL\n");
			free_pages(tmp_slab->page,1);
			kfree(tmp_slab);
			return NULL;
		}
	
		memset(tmp_slab->color_map,0,tmp_slab->color_length);	

		list_add_to_behind(&slab_cache->cache_pool->list,&tmp_slab->list);

		slab_cache->total_free  += tmp_slab->color_count;	
		//	Allocate memory form new struct Slab
		for(j = 0;j < tmp_slab->color_count;j++)
		{
			if( (*(tmp_slab->color_map + (j >> 6)) & (1UL << (j % 64))) == 0 )
			{
				*(tmp_slab->color_map + (j >> 6)) |= 1UL << (j % 64);
			
				tmp_slab->using_count++;
				tmp_slab->free_count--;

				slab_cache->total_using++;
				slab_cache->total_free--;
				
				if(slab_cache->constructor != NULL)
				{
					return slab_cache->constructor((char *)tmp_slab->Vaddress + slab_cache->size * j,arg);
				}
				else
				{			
					return (void *)((char *)tmp_slab->Vaddress + slab_cache->size * j);
				}		
			}
		}
	}
	else
	{
		do
		{
			if(slab_p->free_count == 0)
			{
				slab_p = container_of(list_next(&slab_p->list),struct Slab,list);
				continue;
			}
		
			for(j = 0;j < slab_p->color_count;j++)
			{
				
				if(*(slab_p->color_map + (j >> 6)) == 0xffffffffffffffffUL)
				{
					j += 63;
					continue;
				}	
				
				if( (*(slab_p->color_map + (j >> 6)) & (1UL << (j % 64))) == 0 )
				{
					*(slab_p->color_map + (j >> 6)) |= 1UL << (j % 64);
				
					slab_p->using_count++;
					slab_p->free_count--;

					slab_cache->total_using++;
					slab_cache->total_free--;
					
					if(slab_cache->constructor != NULL)
					{
						return slab_cache->constructor((char *)slab_p->Vaddress + slab_cache->size * j,arg);
					}
					else
					{
						return (void *)((char *)slab_p->Vaddress + slab_cache->size * j);
					}
				}
			}
		}while(slab_p != slab_cache->cache_pool);		
	}

	color_printk(RED,BLACK,"slab_malloc() ERROR: can`t alloc\n");
	if(tmp_slab != NULL)
	{
		list_del(&tmp_slab->list);
		kfree(tmp_slab->color_map);
		page_clean(tmp_slab->page);
		free_pages(tmp_slab->page,1);
		kfree(tmp_slab);
	}

	return NULL;
}

/*
*	Through slab memory pool to free Slab object
*/

unsigned long slab_free(struct Slab_cache * slab_cache,void * address,unsigned long arg)
{
	
	struct Slab * slab_p = slab_cache->cache_pool;
	int index = 0;

	do
	{	
		// wheather it within the management scope of the slab memory pool
		if(slab_p->Vaddress <= address && address < slab_p->Vaddress + PAGE_2M_SIZE)
		{
			// Reset the color_map index bit
			index = (address - slab_p->Vaddress) / slab_cache->size;
			*(slab_p->color_map + (index >> 6)) ^= 1UL << index % 64;
			slab_p->free_count++;
			slab_p->using_count--;

			slab_cache->total_using--;
			slab_cache->total_free++;
			
			if(slab_cache->destructor != NULL)
			{
				slab_cache->destructor((char *)slab_p->Vaddress + slab_cache->size * index,arg);
			}
			//	The number of free objects in the memory pool exceeds 1.5 of the using objects，free this struct Slab
			if((slab_p->using_count == 0) && (slab_cache->total_free >= slab_p->color_count * 3 / 2))
			{
				list_del(&slab_p->list);
				slab_cache->total_free -= slab_p->color_count;

				kfree(slab_p->color_map);
				
				page_clean(slab_p->page);
				free_pages(slab_p->page,1);
				kfree(slab_p);				
			}

			return 1;
		}
		else
		{
			// Find through list to find other Slab obiect
			slab_p = container_of(list_next(&slab_p->list),struct Slab,list);
			continue;	
		}		

	}while(slab_p != slab_cache->cache_pool);

	color_printk(RED,BLACK,"slab_free() ERROR: address not in slab\n");

	return 0;
}

/*
*	Init Slab memory pools，increase end_of_struct to manage memory
*/

unsigned long slab_init()
{
	struct Page * page = NULL;
	unsigned long * virtual = NULL; // get a free page and set to empty page table and return the virtual address
	unsigned long i,j;

	unsigned long tmp_address = memory_management_struct.end_of_struct;

	for(i = 0;i < 16;i++) 
	{
		kmalloc_cache_size[i].cache_pool = (struct Slab *)memory_management_struct.end_of_struct;
		memory_management_struct.end_of_struct = memory_management_struct.end_of_struct + sizeof(struct Slab) + sizeof(long) * 10;

		list_init(&kmalloc_cache_size[i].cache_pool->list);	

	// Init sizeof struct Slab of cache size

		kmalloc_cache_size[i].cache_pool->using_count = 0;
		kmalloc_cache_size[i].cache_pool->free_count  = PAGE_2M_SIZE / kmalloc_cache_size[i].size;

		kmalloc_cache_size[i].cache_pool->color_length =((PAGE_2M_SIZE / kmalloc_cache_size[i].size + sizeof(unsigned long) * 8 - 1) >> 6) << 3;

		kmalloc_cache_size[i].cache_pool->color_count = kmalloc_cache_size[i].cache_pool->free_count;

		kmalloc_cache_size[i].cache_pool->color_map = (unsigned long *)memory_management_struct.end_of_struct;
	
		memory_management_struct.end_of_struct = (unsigned long)(memory_management_struct.end_of_struct + kmalloc_cache_size[i].cache_pool->color_length + sizeof(long) * 10) & ( ~ (sizeof(long) - 1));

		memset(kmalloc_cache_size[i].cache_pool->color_map,0xff,kmalloc_cache_size[i].cache_pool->color_length);

		for(j = 0;j < kmalloc_cache_size[i].cache_pool->color_count;j++)
			*(kmalloc_cache_size[i].cache_pool->color_map + (j >> 6)) ^= 1UL << j % 64;

		kmalloc_cache_size[i].total_free = kmalloc_cache_size[i].cache_pool->color_count;
		kmalloc_cache_size[i].total_using = 0;

	}
	
	//	Init page for kernel code and memory management struct

	i = Virt_To_Phy(memory_management_struct.end_of_struct) >> PAGE_2M_SHIFT;

	for(j = PAGE_2M_ALIGN(Virt_To_Phy(tmp_address)) >> PAGE_2M_SHIFT;j <= i;j++)
	{
		page =  memory_management_struct.pages_struct + j;
		*(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |= 1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
		page->zone_struct->page_using_count++;
		page->zone_struct->page_free_count--;
		page_init(page,PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);
	}

	color_printk(ORANGE,BLACK,"2.memory_management_struct.bits_map:%#018lx\tzone_struct->page_using_count:%d\tzone_struct->page_free_count:%d\n",*memory_management_struct.bits_map,memory_management_struct.zones_struct->page_using_count,memory_management_struct.zones_struct->page_free_count);
	//	Allocate 16 pages for Slab memory pools
	for(i = 0;i < 16;i++)
	{
		virtual = (unsigned long *)((memory_management_struct.end_of_struct + PAGE_2M_SIZE * i + PAGE_2M_SIZE - 1) & PAGE_2M_MASK);
		page = Virt_To_2M_Page(virtual);

		*(memory_management_struct.bits_map + ((page->PHY_address >> PAGE_2M_SHIFT) >> 6)) |= 1UL << (page->PHY_address >> PAGE_2M_SHIFT) % 64;
		page->zone_struct->page_using_count++;
		page->zone_struct->page_free_count--;

		page_init(page,PG_PTable_Maped | PG_Kernel_Init | PG_Kernel);

		kmalloc_cache_size[i].cache_pool->page = page;
		kmalloc_cache_size[i].cache_pool->Vaddress = virtual;
	}

	color_printk(ORANGE,BLACK,"3.memory_management_struct.bits_map:%#018lx\tzone_struct->page_using_count:%d\tzone_struct->page_free_count:%d\n",*memory_management_struct.bits_map,memory_management_struct.zones_struct->page_using_count,memory_management_struct.zones_struct->page_free_count);

	color_printk(ORANGE,BLACK,"start_code:%#018lx,end_code:%#018lx,end_data:%#018lx,end_brk:%#018lx,end_of_struct:%#018lx\n",memory_management_struct.start_code,memory_management_struct.end_code,memory_management_struct.end_data,memory_management_struct.end_brk, memory_management_struct.end_of_struct);

	return 1;
}

/*
*	Init page table
*/

void pagetable_init()
{
	unsigned long i,j;
	unsigned long * tmp = NULL;

	Global_CR3 = Get_gdt();
	//	2MB physical page base address
	//	PAT = 1
	tmp = (unsigned long *)(((unsigned long)Phy_To_Virt((unsigned long)Global_CR3 & (~ 0xfffUL))) + 8 * 256);
		
	color_printk(YELLOW,BLACK,"1:%#018lx,%#018lx\t\t\n",(unsigned long)tmp,*tmp);

	tmp = Phy_To_Virt(*tmp & (~0xfffUL));

	color_printk(YELLOW,BLACK,"2:%#018lx,%#018lx\t\t\n",(unsigned long)tmp,*tmp);

	tmp = Phy_To_Virt(*tmp & (~0xfffUL));

	color_printk(YELLOW,BLACK,"3:%#018lx,%#018lx\t\t\n",(unsigned long)tmp,*tmp);

	for(i = 0;i < memory_management_struct.zones_size;i++)
	{
		struct Zone * z = memory_management_struct.zones_struct + i;
		struct Page * p = z->pages_group;

		if(ZONE_UNMAPED_INDEX && i == ZONE_UNMAPED_INDEX)
			break;

		for(j = 0;j < z->pages_length ; j++,p++)
		{
			//	Get PML4E address
			tmp = (unsigned long *)(((unsigned long)Phy_To_Virt((unsigned long)Global_CR3 & (~ 0xfffUL))) + (((unsigned long)Phy_To_Virt(p->PHY_address) >> PAGE_GDT_SHIFT) & 0x1ff) * 8);
			//	If no configuration mapping is performed, the page table entry is set
			if(*tmp == 0)
			{			
				unsigned long * virtual = kmalloc(PAGE_4K_SIZE,0);
				set_mpl4t(tmp,mk_mpl4t(Virt_To_Phy(virtual),PAGE_KERNEL_GDT));
			}
			//	Get PDPT address
			tmp = (unsigned long *)((unsigned long)Phy_To_Virt(*tmp & (~ 0xfffUL)) + (((unsigned long)Phy_To_Virt(p->PHY_address) >> PAGE_1G_SHIFT) & 0x1ff) * 8);
			
			if(*tmp == 0)
			{
				unsigned long * virtual = kmalloc(PAGE_4K_SIZE,0);
				set_pdpt(tmp,mk_pdpt(Virt_To_Phy(virtual),PAGE_KERNEL_Dir));
			}
			//	Get PDT address
			tmp = (unsigned long *)((unsigned long)Phy_To_Virt(*tmp & (~ 0xfffUL)) + (((unsigned long)Phy_To_Virt(p->PHY_address) >> PAGE_2M_SHIFT) & 0x1ff) * 8);

			set_pdt(tmp,mk_pdt(p->PHY_address,PAGE_KERNEL_Page));

			if(j % 50 == 0)
				color_printk(GREEN,BLACK,"@:%#018lx,%#018lx\t\n",(unsigned long)tmp,*tmp);
		}
	}

	flush_tlb();
}