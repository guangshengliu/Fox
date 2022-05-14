#ifndef __SCHEDULE_H__

#define __SCHEDULE_H__

#include "task.h"

struct schedule
{
	//	Number of task in the queue
	long running_task_count;
	//	Task executable time slice
	long CPU_exec_task_jiffies;
	//	Task ready queue head
	struct task_struct task_queue;
};

extern struct schedule task_schedule[NR_CPUS];

void schedule();
void schedule_init();
struct task_struct *get_next_task();
void insert_task_queue(struct task_struct *tsk);

#endif
