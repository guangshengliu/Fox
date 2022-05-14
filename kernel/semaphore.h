#ifndef __SEMAPHORE_H__

#define __SEMAPHORE_H__

#include "atomic.h"
#include "lib.h"
#include "task.h"
#include "schedule.h"
#include "waitqueue.h"

typedef struct 
{
	//	Statistical resource atomic variable
	atomic_T counter;
	//	Waiting queue
	wait_queue_T wait;
} semaphore_T;


void semaphore_init(semaphore_T * semaphore,unsigned long count);

void semaphore_up(semaphore_T * semaphore);

void semaphore_down(semaphore_T * semaphore);

#endif
