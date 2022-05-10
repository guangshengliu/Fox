#ifndef __CPU_H__

#define __CPU_H__

#define NR_CPUS 8

/*
*	Mop和Sop参数用于向CPUID指令传递主功能号和子功能号
*	查询结果的返回值保存到指针变量a，b，c和d指向的内存中
*/
static inline void get_cpuid(unsigned int Mop,unsigned int Sop,unsigned int * a,unsigned int * b,unsigned int * c,unsigned int * d)
{
	__asm__ __volatile__	(	"cpuid	\n\t"
					:"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d)
					:"0"(Mop),"2"(Sop)
				);
}

/*

*/

void init_cpu(void);


#endif
