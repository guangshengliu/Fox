#include "timer.h"
#include "softirq.h"
#include "printk.h"
#include "lib.h"
#include "memory.h"

/*
*	Init timer list
*/

void init_timer(struct timer_list * timer,void (* func)(void * data),void *data,unsigned long expire_jiffies)
{
	list_init(&timer->list);
	timer->func = func;
	timer->data = data;
	timer->expire_jiffies = jiffies + expire_jiffies;
}

/*
*	Increase timer task
*/

void add_timer(struct timer_list * timer)
{
	struct timer_list * tmp = container_of(list_next(&timer_list_head.list),struct timer_list,list);

	if(list_is_empty(&timer_list_head.list))
	{
		
	}
	else
	{
		while(tmp->expire_jiffies < timer->expire_jiffies)
			tmp = container_of(list_next(&tmp->list),struct timer_list,list);
	}

	list_add_to_behind(&tmp->list,&timer->list);
}

/*
*	Delete timer task
*/

void del_timer(struct timer_list * timer)
{
	list_del(&timer->list);
}

void test_timer(void * data)
{
	color_printk(BLUE,WHITE,"test_timer");
}

/*
*	Timer interupt register
*/

void timer_init()
{
	struct timer_list *tmp = NULL;
	jiffies = 0;
	init_timer(&timer_list_head,NULL,NULL,-1UL);
	register_softirq(0,&do_timer,NULL);

	tmp = (struct timer_list *)kmalloc(sizeof(struct timer_list),0);
	// Trigger interrupt after 5s
	init_timer(tmp,&test_timer,NULL,5);
	add_timer(tmp);
}

/*
*	Timer soft interrupt processing function
*/

void do_timer(void * data)
{
	struct timer_list * tmp = container_of(list_next(&timer_list_head.list),struct timer_list,list);
	while((!list_is_empty(&timer_list_head.list)) && (tmp->expire_jiffies <= jiffies))
	{
		del_timer(tmp);
		tmp->func(tmp->data);
		tmp = container_of(list_next(&timer_list_head.list),struct timer_list,list);
	}

	color_printk(RED,WHITE,"(HPET:%ld)",jiffies);
}
