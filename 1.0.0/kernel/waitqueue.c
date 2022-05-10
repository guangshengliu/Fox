#include "waitqueue.h"
#include "task.h"
#include "lib.h"
//#include "schedule.h"

/*
*	Record pending task PCB
*/

void wait_queue_init(wait_queue_T * wait_queue,struct task_struct *tsk)
{
	list_init(&wait_queue->wait_list);
	wait_queue->tsk = tsk;
}

/*
*	Set task sleep 0n and insert it to wait_queue
*/

void sleep_on(wait_queue_T * wait_queue_head)
{
	wait_queue_T wait;
	wait_queue_init(&wait,current);
	current->state = TASK_UNINTERRUPTIBLE;
	list_add_to_before(&wait_queue_head->wait_list,&wait.wait_list);

	schedule();
}

/*
*	Set task sleep 0n and insert it to wait_queue(Beacuse of interruption)
*/

void interruptible_sleep_on(wait_queue_T *wait_queue_head)
{
	wait_queue_T wait;
	wait_queue_init(&wait,current);
	current->state = TASK_INTERRUPTIBLE;
	list_add_to_before(&wait_queue_head->wait_list,&wait.wait_list);

	schedule();
}

/*
*	Wake task from wait queue and insert it to ready queue
*/

void wakeup(wait_queue_T * wait_queue_head,long state)
{
	wait_queue_T * wait = NULL;

	if(list_is_empty(&wait_queue_head->wait_list))
		return;

	wait = container_of(list_next(&wait_queue_head->wait_list),wait_queue_T,wait_list);

	if(wait->tsk->state & state)
	{
		list_del(&wait->wait_list);
		wakeup_process(wait->tsk);
	}
}


