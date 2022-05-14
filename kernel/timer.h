#ifndef __TIMER_H__

#define __TIMER_H__

#include "lib.h"

//  record the number of interrupts produced by timer
unsigned long volatile jiffies = 0;

// Timer task queue
struct timer_list
{
	struct List list;
	unsigned long expire_jiffies;   //  expect interrupts number
	void (* func)(void * data);
	void *data;
};

//  List header of timer function
struct timer_list timer_list_head;

void init_timer(struct timer_list * timer,void (* func)(void * data),void *data,unsigned long expire_jiffies);

void add_timer(struct timer_list * timer);

void del_timer(struct timer_list * timer);

void timer_init();

void do_timer();

#endif
