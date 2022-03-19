CFLAGS := -mcmodel=large -fno-builtin -m64

ASFLAGS := --64

all: system_tmp kallsyms.o
	ld -b elf64-x86-64 -z muldefs -o system head.o entry.o main.o printk.o trap.o memory.o interrupt.o task.o sys.o syscalls.o kallsyms.o cpu.o -T Kernel.lds 
	objcopy -I elf64-x86-64 -S -R ".eh_frame" -R ".comment" -O binary system kernel.bin

system_tmp: head.o entry.o main.o printk.o trap.o memory.o interrupt.o task.o sys.o syscalls.o cpu.o
	ld -b elf64-x86-64 -z muldefs -o system_tmp head.o entry.o main.o printk.o trap.o memory.o interrupt.o task.o sys.o syscalls.o cpu.o -T Kernel.lds 

head.o:	head.S
	gcc -E  head.S > head.s
	as $(ASFLAGS) -o head.o head.s

entry.o: entry.S
	gcc -E  entry.S > entry.s
	as $(ASFLAGS) -o entry.o entry.s
	
main.o:	main.c
	gcc  $(CFLAGS) -c main.c -fno-stack-protector
	
printk.o: printk.c
	gcc  $(CFLAGS) -c printk.c -fno-stack-protector

trap.o: trap.c
	gcc  $(CFLAGS) -c trap.c

memory.o: memory.c
	gcc  $(CFLAGS) -c memory.c

interrupt.o: interrupt.c
	gcc  $(CFLAGS) -c interrupt.c
	
task.o: task.c
	gcc  $(CFLAGS) -c task.c	-fno-stack-protector -fno-pic
sys.o: sys.c
	gcc  $(CFLAGS) -c sys.c	

syscalls.o: syscalls.c
	gcc  $(CFLAGS) -c syscalls.c	

kallsyms.o: kallsyms.c system_tmp
	gcc -o kallsyms kallsyms.c
	nm -n system_tmp | ./kallsyms > kallsyms.S
	gcc -c kallsyms.S

cpu.o: cpu.c
	gcc  $(CFLAGS) -c cpu.c	-fno-stack-protector

clean:
	rm -rf *.o *.s~ *.s *.S~ *.c~ *.h~ system system_tmp Makefile~ Kernel.lds~ kernel.bin kallsyms kallsyms.o kallsyms.S
