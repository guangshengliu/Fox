#ifndef __BLOCK_H__

#define __BLOCK_H__

#include "waitqueue.h"

struct block_buffer_node
{
	unsigned int count;
	unsigned char cmd;
	unsigned long LBA;
	unsigned char * buffer;
	void(* end_handler)(unsigned long nr, unsigned long parameter);

	wait_queue_T wait_queue;
};

struct request_queue
{
	wait_queue_T wait_queue_list;
	struct block_buffer_node *in_using;
	long block_request_count;
};

struct block_device_operation
{
	long (* open)();
	long (* close)();
	long (* ioctl)(long cmd,long arg);
	long (* transfer)(long cmd,unsigned long blocks,long count,unsigned char * buffer);
};

#endif
