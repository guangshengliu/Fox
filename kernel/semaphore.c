#include "atomic.h"
#include "lib.h"
#include "task.h"
#include "schedule.h"
#include "waitqueue.h"
#include "semaphore.h"


/*
*	Init semaphore
*/

void semaphore_init(semaphore_T * semaphore,unsigned long count)
{
	atomic_set(&semaphore->counter,count);
	wait_queue_init(&semaphore->wait,NULL);
}

/*
*	Wake up task from suspended queue ,insert it to Waiting queue
*/

void __up(semaphore_T * semaphore)
{
	wait_queue_T * wait = container_of(list_next(&semaphore->wait.wait_list),wait_queue_T,wait_list);

	list_del(&wait->wait_list);
	wait->tsk->state = TASK_RUNNING;
	insert_task_queue(wait->tsk);
}

/*
*	Free semaphore resources
*/

void semaphore_up(semaphore_T * semaphore)
{
	if(list_is_empty(&semaphore->wait.wait_list))
		atomic_inc(&semaphore->counter);
	else
		__up(semaphore);
}

/*
*	Semaphore no resources, add it to wait queue
*/

void __down(semaphore_T * semaphore)
{
	wait_queue_T wait;
	wait_queue_init(&wait,current);
	current->state = TASK_UNINTERRUPTIBLE;
	list_add_to_before(&semaphore->wait.wait_list,&wait.wait_list);
	//	Transfer of processor use right
	schedule();
}

/*
*	Get semaphore resources
*/

void semaphore_down(semaphore_T * semaphore)
{
	if(atomic_read(&semaphore->counter) > 0)
		atomic_dec(&semaphore->counter);
	else
		__down(semaphore);
}
