#include "keyboard.h"
#include "lib.h"
#include "interrupt.h"
#include "APIC.h"
#include "memory.h"
#include "printk.h"

/*

*/

struct keyboard_inputbuffer * p_kb = NULL;
static int shift_l,shift_r,ctrl_l,ctrl_r,alt_l,alt_r;

void keyboard_handler(unsigned long nr, unsigned long parameter, struct pt_regs * regs)
{
	unsigned char x;
	x = io_in8(0x60);
	color_printk(WHITE,BLACK,"(K:%02x)",x);
	//	p_head must be less than p_kb->buf + KB_BUF_SIZE
	//	Which is circular queue
	if(p_kb->p_head == p_kb->buf + KB_BUF_SIZE)
		p_kb->p_head = p_kb->buf;

	*p_kb->p_head = x;
	p_kb->count++;
	p_kb->p_head ++;
}

/*

*/

unsigned char get_scancode()
{
	unsigned char ret  = 0;
	// 	Wait keyboard send messages
	if(p_kb->count == 0)
		while(!p_kb->count)
			nop();
	
	if(p_kb->p_tail == p_kb->buf + KB_BUF_SIZE)	
		p_kb->p_tail = p_kb->buf;

	ret = *p_kb->p_tail;
	p_kb->count--;
	p_kb->p_tail++;

	return ret;
}

/*
*	Analysis keyboard code function
*/

void analysis_keycode()
{
	unsigned char x = 0;
	int i;	
	int key = 0;	
	int make = 0;

	x = get_scancode();
	//	The first type of keyboard scan code
	if(x == 0xE1)	//	pause break;
	{
		key = PAUSEBREAK;
		for(i = 1;i<6;i++)
			if(get_scancode() != pausebreak_scode[i])
			{
				key = 0;
				break;
			}
	}
	//	The second type of keyboard scan code
	else if(x == 0xE0) //	print screen
	{
		x = get_scancode();

		switch(x)
		{
			case 0x2A: //	press printscreen
		
				if(get_scancode() == 0xE0)
					if(get_scancode() == 0x37)
					{
						key = PRINTSCREEN;
						make = 1;
					}
				break;

			case 0xB7: //	UNpress printscreen
		
				if(get_scancode() == 0xE0)
					if(get_scancode() == 0xAA)
					{
						key = PRINTSCREEN;
						make = 0;
					}
				break;

			case 0x1d: // press right ctrl
		
				ctrl_r = 1;
				key = OTHERKEY;
				break;
			case 0x9d: // UNpress right ctrl
		
				ctrl_r = 0;
				key = OTHERKEY;
				break;
			
			case 0x38: // press right alt
		
				alt_r = 1;
				key = OTHERKEY;
				break;
			case 0xb8: // UNpress right alt
		
				alt_r = 0;
				key = OTHERKEY;
				break;		

			case 0x35: // press /
		
				key = OTHERKEY;
				break;
			case 0xb5: // UNpress /
		
				key = OTHERKEY;
				break;
			
			case 0x1c: // press Enter
		
				key = OTHERKEY;
				break;
			case 0x9c: // UNpress Enter
		
				key = OTHERKEY;
				break;
			
			case 0x46: // press Left
		
				key = OTHERKEY;
				break;
			case 0xc6: // UNpress Left
		
				key = OTHERKEY;
				break;

			case 0x47: // press Home
		
				key = OTHERKEY;
				break;
			case 0xc7: // UNpress Home
		
				key = OTHERKEY;
				break;
			
			case 0x48: // press Up
		
				key = OTHERKEY;
				break;
			case 0xc8: // UNpress Up
		
				key = OTHERKEY;
				break;
			
			case 0x49: // press Page Up
		
				key = OTHERKEY;
				break;
			case 0xc9: // UNpress Page Up
		
				key = OTHERKEY;
				break;

			case 0x4d: // press Right
		
				key = OTHERKEY;
				break;
			case 0xcd: // UNpress Right
		
				key = OTHERKEY;
				break;

			case 0x4f: // press End
		
				key = OTHERKEY;
				break;
			case 0xcf: // UNpress End
		
				key = OTHERKEY;
				break;
			
			case 0x50: // press Down
		
				key = OTHERKEY;
				break;
			case 0xd0: // UNpress Down
		
				key = OTHERKEY;
				break;

			case 0x51: // press Page Down
		
				key = OTHERKEY;
				break;
			case 0xd1: // UNpress Page Down
		
				key = OTHERKEY;
				break;

			case 0x52: // press Insert
		
				key = OTHERKEY;
				break;
			case 0xd2: // UNpress Insert
		
				key = OTHERKEY;
				break;
			
			case 0x53: // press Delete
		
				key = OTHERKEY;
				break;
			case 0xd3: // UNpress Delete
		
				key = OTHERKEY;
				break;
			
			default:
				key = OTHERKEY;
				break;
		}
		
	}
	//	The third type of keyboard scan code
	if(key == 0)
	{
		unsigned int * keyrow = NULL;
		int column = 0;
		//	press or lift ？
		make = (x & FLAG_BREAK ? 0:1);

		keyrow = &keycode_map_normal[(x & 0x7F) * MAP_COLS];

		if(shift_l || shift_r)
			column = 1;
		//	Keep Shift code or unShift code ?
		key = keyrow[column];
		//	Whether the button is functional ？
		switch(x & 0x7F)
		{
			case 0x2a:	//SHIFT_L:
				shift_l = make;
				key = 0;
				break;

			case 0x36:	//SHIFT_R:
				shift_r = make;
				key = 0;
				break;

			case 0x1d:	//CTRL_L:
				ctrl_l = make;
				key = 0;
				break;

			case 0x38:	//ALT_L:
				alt_l = make;
				key = 0;
				break;

			default:
				if(!make)
					key = 0;
				break;
		}			

		if(key)
			color_printk(RED,BLACK,"(K:%c)\t",key);
	}
}


hw_int_controller keyboard_int_controller = 
{
	.enable = IOAPIC_enable,
	.disable = IOAPIC_disable,
	.install = IOAPIC_install,
	.uninstall = IOAPIC_uninstall,
	.ack = IOAPIC_edge_ack,
};

/*

*/


void keyboard_init()
{
	struct IO_APIC_RET_entry entry;
	unsigned long i,j;
	//	Malloc sizes to storage struct keyboard_inputbuffer
	p_kb = (struct keyboard_inputbuffer *)kmalloc(sizeof(struct keyboard_inputbuffer),0);
	
	p_kb->p_head = p_kb->buf;
	p_kb->p_tail = p_kb->buf;
	p_kb->count  = 0;
	memset(p_kb->buf,0,KB_BUF_SIZE);

	entry.vector = 0x21;
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

	wait_KB_write();
	io_out8(PORT_KB_CMD,KBCMD_WRITE_CMD);
	wait_KB_write();
	io_out8(PORT_KB_DATA,KB_INIT_MODE);

	for(i = 0;i<1000;i++)
		for(j = 0;j<1000;j++)
			nop();
	
	shift_l = 0;
	shift_r = 0;
	ctrl_l  = 0;
	ctrl_r  = 0;
	alt_l   = 0;
	alt_r   = 0;

	register_irq(0x21, &entry , &keyboard_handler, (unsigned long)p_kb, &keyboard_int_controller, "ps/2 keyboard");
}

/*

*/

void keyboard_exit()
{
	unregister_irq(0x21);
	kfree((unsigned long *)p_kb);
}
