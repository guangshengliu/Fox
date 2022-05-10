#ifndef __WAITQUEUE_H__

#define __WAITQUEUE_H__

#include "lib.h"

typedef struct
{
	//	Linked list of suspended task
	struct List wait_list;
	//	Suspended task
	struct task_struct *tsk;
} wait_queue_T;

void wait_queue_init(wait_queue_T * wait_queue,struct task_struct *tsk);

void sleep_on(wait_queue_T * wait_queue_head);

void interruptible_sleep_on(wait_queue_T *wait_queue_head);

void wakeup(wait_queue_T * wait_queue_head,long state);

#endif
