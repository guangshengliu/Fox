#include <stdarg.h>
#include "printk.h"
#include "lib.h"
#include "linkage.h"
#include "memory.h"
#include "spinlock.h"

static char buf[4096]={0};

struct position Pos=
{
	.XResolution=1366,
	.YResolution=768,

	.XPosition=0,
	.YPosition=0,

	.XCharSize=8,
	.YCharSize=16,

	.FB_addr=(int *)0xffff800003000000,
	.FB_length=0x408000,
};

/*
*	VBE buffer address map tp linear address
*	According to pagetable_init()
*/

void frame_buffer_init()
{
	////re init frame buffer;
	unsigned long i;
	unsigned long * tmp;
	unsigned long * tmp1;
	unsigned int * FB_addr = (unsigned int *)Phy_To_Virt(boot_para_info->Graphics_Info.FrameBufferBase);

	tmp = Phy_To_Virt((unsigned long *)((unsigned long)Get_gdt() & (~ 0xfffUL)) + (((unsigned long)FB_addr >> PAGE_GDT_SHIFT) & 0x1ff));
	if (*tmp == 0)
	{
		unsigned long * virtual = kmalloc(PAGE_4K_SIZE,0);
		memset(virtual,0,PAGE_4K_SIZE);
		set_mpl4t(tmp,mk_mpl4t(Virt_To_Phy(virtual),PAGE_KERNEL_GDT));
	}

	tmp = Phy_To_Virt((unsigned long *)(*tmp & (~ 0xfffUL)) + (((unsigned long)FB_addr >> PAGE_1G_SHIFT) & 0x1ff));
	if(*tmp == 0)
	{
		unsigned long * virtual = kmalloc(PAGE_4K_SIZE,0);
		memset(virtual,0,PAGE_4K_SIZE);
		set_pdpt(tmp,mk_pdpt(Virt_To_Phy(virtual),PAGE_KERNEL_Dir));
	}
	
	for(i = 0;i < Pos.FB_length;i += PAGE_2M_SIZE)
	{
		tmp1 = Phy_To_Virt((unsigned long *)(*tmp & (~ 0xfffUL)) + (((unsigned long)((unsigned long)FB_addr + i) >> PAGE_2M_SHIFT) & 0x1ff));
	
		unsigned long phy = boot_para_info->Graphics_Info.FrameBufferBase + i;
		set_pdt(tmp1,mk_pdt(phy,PAGE_KERNEL_Page | PAGE_PWT | PAGE_PCD));
	}

	Pos.FB_addr = (unsigned int *)Phy_To_Virt(boot_para_info->Graphics_Info.FrameBufferBase);

	flush_tlb();
}

/*
* putchar函数参数：帧缓存线性地址，行分辨率，屏幕行像素点位置，字体颜色，字体背景色和字符位图
*/

void putchar(unsigned int * fb,int Xsize,int x,int y,unsigned int FRcolor,unsigned int BKcolor,unsigned char font)
{
	int i = 0,j = 0;
	unsigned int * addr = NULL;
	unsigned char * fontp = NULL;
	int testval = 0;
	fontp = font_ascii[font];

	// 从字符首像素地址开始，将字体颜色和背景色的数值按照字符位图的描述
	// 填充到相应的线性地址空间中
	for(i = 0; i< 16;i++)
	{
		// 待显示字符矩阵的起始线性地址
		addr = fb + Xsize * ( y + i ) + x;
		testval = 0x100;
		for(j = 0;j < 8;j ++)		
		{
			testval = testval >> 1;
			if(*fontp & testval)
				*addr = FRcolor;
			else
				*addr = BKcolor;
			addr++;
		}
		fontp++;
	}
}


/*
* 将字符串转化为整型数字
*/

int skip_atoi(const char **s)
{
	int i=0;
	// i是为了支持大于9的字符串转化
	while (is_digit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

/*
* 将长整型变量转换成指定进制规格(由base指定进制数)的字符串
*/

static char * number(char * str, long num, int base, int size, int precision, int type)
{
	char c,sign,tmp[50];
	const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	if (type&SMALL) digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	if (type&LEFT) type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ' ;
	sign = 0;
	if (type&SIGN && num < 0) {
		sign='-';
		num = -num;
	} else
		sign=(type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);
	if (sign) size--;
	if (type & SPECIAL)
		if (base == 16) size -= 2;
		else if (base == 8) size--;
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num!=0)
		// 负责将整数值转化字符串
		tmp[i++]=digits[do_div(num,base)];
	if (i > precision) precision=i;
	size -= precision;
	if (!(type & (ZEROPAD + LEFT)))
		while(size-- > 0)
			*str++ = ' ';
	if (sign)
		*str++ = sign;
	if (type & SPECIAL)
		if (base == 8)
			*str++ = '0';
		else if (base == 16) 
		{
			*str++ = '0';
			*str++ = digits[33];
		}
	if (!(type & LEFT))
		while(size-- > 0)
			*str++ = c;

	while(i < precision--)
		*str++ = '0';
	// 将tmp数组中的字符倒序列插入到显示缓存区
	while(i-- > 0)
		*str++ = tmp[i];
	while(size-- > 0)
		*str++ = ' ';
	return str;
}


/*
* 解析color_printk函数提供的格式化字符串及其参数
* 将格式化后的结果保存到一个4096B的缓冲区buf，并返回字符串长度
*/

int vsprintf(char * buf,const char *fmt, va_list args)
{
	char * str,*s;
	int flags;
	int field_width;
	int precision;
	int len,i;

	int qualifier;		/* 'h', 'l', 'L' or 'Z' for integer fields */

	for(str = buf; *fmt; fmt++)
	{
		// 如果字符不为‘%’就认为它是个可显示字符
		if(*fmt != '%')
		{
			*str++ = *fmt;		// 向缓冲区写入字符
			continue;
		}
		flags = 0;
		repeat:
			fmt++;
			switch(*fmt)
			{
				case '-':flags |= LEFT;	
				goto repeat;
				case '+':flags |= PLUS;	
				goto repeat;
				case ' ':flags |= SPACE;	
				goto repeat;
				case '#':flags |= SPECIAL;	
				goto repeat;
				case '0':flags |= ZEROPAD;	
				goto repeat;
			}

			/* 计算数据区域的宽度 */

			field_width = -1;
			if(is_digit(*fmt))
				field_width = skip_atoi(&fmt);
			// 如果字符为'*',那么数据区域的宽度由可变参数提供
			else if(*fmt == '*')
			{
				fmt++;
				// 检索函数参数列表中类型为 type 的下一个参数
				field_width = va_arg(args, int);
				// 根据可变参数数值可判断数据区域的对齐方式
				if(field_width < 0)
				{
					field_width = -field_width;
					flags |= LEFT;
				}
			}
			
			/* 提取出显示数据的精度 */

			precision = -1;
			// 如果数据区域的宽度后面跟有字符'.'，说明其后的数值是显示数据的精度
			if(*fmt == '.')
			{
				fmt++;
				if(is_digit(*fmt))
					precision = skip_atoi(&fmt);
				else if(*fmt == '*')
				{
					fmt++;
					precision = va_arg(args, int);
				}
				if(precision < 0)
					precision = 0;
			}

			/* 检测显示数据的规格 */
			
			qualifier = -1;
			if(*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt == 'Z')
			{	
				qualifier = *fmt;
				fmt++;
			}
							
			switch(*fmt)
			{
				// 匹配出格式符c，程序将可变参数转换为一个字符
				// 根据数据区域的宽度和对齐方式填充空格符，即为%c格式符功能
				case 'c':

					if(!(flags & LEFT))
						while(--field_width > 0)
							*str++ = ' ';
					*str++ = (unsigned char)va_arg(args, int);
					while(--field_width > 0)
						*str++ = ' ';
					break;

				// 字符串显示功能，即%s格式符的功能
				case 's':
				
					s = va_arg(args,char *);
					if(!s)
						s = '\0';
					// 把字符串的长度与显示精度进行对比
					len = strlen(s);
					if(precision < 0)
						precision = len;
					else if(len > precision)
						len = precision;
					// 根据数据区域的宽度截取待显示字符串的长度并补全空格符
					if(!(flags & LEFT))
						while(len < field_width--)
							*str++ = ' ';
					for(i = 0;i < len ;i++)
						*str++ = *s++;
					while(len < field_width--)
						*str++ = ' ';
					break;

				// 八进制格式化显示功能
				case 'o':
					
					if(qualifier == 'l')
						str = number(str,va_arg(args,unsigned long),8,field_width,precision,flags);
					else
						str = number(str,va_arg(args,unsigned int),8,field_width,precision,flags);
					break;
				
				case 'p':

					if(field_width == -1)
					{
						field_width = 2 * sizeof(void *);
						flags |= ZEROPAD;
					}

					str = number(str,(unsigned long)va_arg(args,void *),16,field_width,precision,flags);
					break;

				case 'x':

					flags |= SMALL;

				// 十六进制格式化显示功能
				case 'X':

					if(qualifier == 'l')
						str = number(str,va_arg(args,unsigned long),16,field_width,precision,flags);
					else
						str = number(str,va_arg(args,unsigned int),16,field_width,precision,flags);
					break;

				case 'd':
				case 'i':

					flags |= SIGN;

				// 十进制格式化显示功能
				case 'u':

					if(qualifier == 'l')
						str = number(str,va_arg(args,unsigned long),10,field_width,precision,flags);
					else
						str = number(str,va_arg(args,unsigned int),10,field_width,precision,flags);
					break;

				// 格式符%n的功能是将已格式化的字符串长度返回给函数的调用者
				case 'n':
					
					if(qualifier == 'l')
					{
						long *ip = va_arg(args,long *);
						*ip = (str - buf);
					}
					else
					{
						int *ip = va_arg(args,int *);
						*ip = (str - buf);
					}
					break;

				case '%':
					
					*str++ = '%';
					break;

				default:

					*str++ = '%';	
					if(*fmt)
						*str++ = *fmt;
					else
						fmt--;
					break;
			}

	}
	*str = '\0';
	return str - buf;
}

/*
* FRcolor：字体颜色	BKcolor：背景颜色 *fmt：字符串
*/

int color_printk(unsigned int FRcolor,unsigned int BKcolor,const char * fmt,...)
{
	int i = 0;
	int count = 0;
	int line = 0;
	va_list args;

	if(get_rflags() & 0x200UL)
	{
		spin_lock(&Pos.printk_lock);
	}

	va_start(args, fmt);
	// 得到字符串长度
	i = vsprintf(buf,fmt, args);
	va_end(args);

	for(count = 0;count < i || line;count++)
	{
		////	add \n \b \t
		if(line > 0)
		{
			count--;
			goto Label_tab;
		}
		// 如果发现某个待显示字符是\n转义字符
		// 光标行数加1，列数设置为0
		if((unsigned char)*(buf + count) == '\n')
		{
			Pos.YPosition++;
			Pos.XPosition = 0;
		}
		// 如果待显示字符是\b转义符
		// 调整列位置并调用putchar函数打印空格符覆盖之前字符
		else if((unsigned char)*(buf + count) == '\b')
		{
			Pos.XPosition--;
			if(Pos.XPosition < 0)
			{
				Pos.XPosition = Pos.XResolution / Pos.XCharSize - 1;
				Pos.YPosition--;
				if(Pos.YPosition < 0)
					Pos.YPosition = Pos.YResolution / Pos.YCharSize - 1;
			}	
			putchar(Pos.FB_addr , Pos.XResolution , Pos.XPosition * Pos.XCharSize , Pos.YPosition * Pos.YCharSize , FRcolor , BKcolor , ' ');	
		}
		// 如果待显示是\t转义符
		// 计算当前光标距离下一个制表位需要填充的空格符数量
		else if((unsigned char)*(buf + count) == '\t')
		{
			line = ((Pos.XPosition + 8) & ~(8 - 1)) - Pos.XPosition;

Label_tab:
			line--;
			putchar(Pos.FB_addr , Pos.XResolution , Pos.XPosition * Pos.XCharSize , Pos.YPosition * Pos.YCharSize , FRcolor , BKcolor , ' ');	
			Pos.XPosition++;
		}
		// 普通字符字节打印
		else
		{
			putchar(Pos.FB_addr , Pos.XResolution , Pos.XPosition * Pos.XCharSize , Pos.YPosition * Pos.YCharSize , FRcolor , BKcolor , (unsigned char)*(buf + count));
			Pos.XPosition++;
		}

		// 负责调整光标的列位置和行位置
		if(Pos.XPosition >= (Pos.XResolution / Pos.XCharSize))
		{
			Pos.YPosition++;
			Pos.XPosition = 0;
		}
		if(Pos.YPosition >= (Pos.YResolution / Pos.YCharSize))
		{
			Pos.YPosition = 0;
		}

	}
	
	if(get_rflags() & 0x200UL)
	{
		spin_unlock(&Pos.printk_lock);
	}
	
	return i;
}
