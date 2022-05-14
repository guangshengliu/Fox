#include "schedule.h"
#include "task.h"
#include "lib.h"
#include "printk.h"
#include "timer.h"
#include "SMP.h"

struct schedule task_schedule[NR_CPUS];

/*
*	Get next task from ready queue
*/

struct task_struct *get_next_task()
{
	struct task_struct * tsk = NULL;

	if(list_is_empty(&task_schedule[SMP_cpu_id()].task_queue.list))
	{
		return init_task[SMP_cpu_id()];
	}

	tsk = container_of(list_next(&task_schedule[SMP_cpu_id()].task_queue.list),struct task_struct,list);
	list_del(&tsk->list);

	task_schedule[SMP_cpu_id()].running_task_count -= 1;

	return tsk;
}

/*
*	Insert task to ready queue
*/

void insert_task_queue(struct task_struct *tsk)
{
	struct task_struct *tmp = NULL;

	if(tsk == init_task[SMP_cpu_id()])
		return ;

	tmp = container_of(list_next(&task_schedule[SMP_cpu_id()].task_queue.list),struct task_struct,list);

	if(list_is_empty(&task_schedule[SMP_cpu_id()].task_queue.list))
	{
	}
	else
	{
		while(tmp->vrun_time < tsk->vrun_time)
			tmp = container_of(list_next(&tmp->list),struct task_struct,list);
	}

	list_add_to_before(&tmp->list,&tsk->list);
	task_schedule[SMP_cpu_id()].running_task_count += 1;
}

/*
*	Schedule task
*/

void schedule()
{
	struct task_struct *tsk = NULL;
	long cpu_id = SMP_cpu_id();
	//	~ means Reverse bit
	//	reset flags
	current->flags &= ~NEED_SCHEDULE;
	//	Get next task from queue
	tsk = get_next_task();

	color_printk(RED,BLACK,"#schedule:%d#\n",jiffies);
	//	In CFS scheduling algorithm, virtual time represents task priority
	//	The virtual running time of the current process is longer than that of the next one
	if(current->vrun_time >= tsk->vrun_time)
	{	
		//	Scheduling caused by expiration of time slice
		if(current->state == TASK_RUNNING)
			insert_task_queue(current);
		//	Time slice equals zero
		if(!task_schedule[cpu_id].CPU_exec_task_jiffies){
			switch(tsk->priority)
			{
				case 0:
				case 1:
					task_schedule[cpu_id].CPU_exec_task_jiffies = 4/task_schedule[cpu_id].running_task_count;
					break;
				case 2:
				default:
					task_schedule[cpu_id].CPU_exec_task_jiffies = 4/task_schedule[cpu_id].running_task_count*3;
					break;
			}
		}
		switch_to(current,tsk);
	}
	//	The virtual running time of the current process is less than that of the next one
	else
	{
		insert_task_queue(tsk);
		//	Reassign time slice
		if(!task_schedule[cpu_id].CPU_exec_task_jiffies)
		{
			switch(tsk->priority)
			{
				case 0:
				case 1:
					task_schedule[cpu_id].CPU_exec_task_jiffies = 4/task_schedule[cpu_id].running_task_count;
					break;
				case 2:
				default:
					task_schedule[cpu_id].CPU_exec_task_jiffies = 4/task_schedule[cpu_id].running_task_count*3;
					break;
			}
		}
	}
}

/*
*	Init schedule
*/

void schedule_init()
{
	int i = 0;
	memset(&task_schedule,0,sizeof(struct schedule) * NR_CPUS);

	for(i = 0;i<NR_CPUS;i++)
	{
		list_init(&task_schedule[i].task_queue.list);
		task_schedule[i].task_queue.vrun_time = 0x7fffffffffffffff;

		task_schedule[i].running_task_count = 1;
		task_schedule[i].CPU_exec_task_jiffies = 4;
	}
}
