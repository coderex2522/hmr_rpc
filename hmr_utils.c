#include "hmr_base.h"
#include "hmr_task.h"
#include "hmr_utils.h"

enum ibv_wr_opcode hmr_get_opcode_from_msg(struct hmr_msg *msg)
{
	enum ibv_wr_opcode retval;
	
	switch (msg->msg_type)
	{
	case HMR_MSG_READ:
		retval=IBV_WR_RDMA_READ;
		break;
	case HMR_MSG_WRITE:
		retval=IBV_WR_RDMA_WRITE;
		break;
	default:
		retval=IBV_WR_SEND;
		break;
	}
	
	return retval;
}

enum ibv_wr_opcode hmr_get_opcode_from_task(struct hmr_task *task)
{
	enum ibv_wr_opcode retval;
		
	switch (task->task_type)
	{
	case HMR_TASK_READ:
		retval=IBV_WR_RDMA_READ;
		break;
	case HMR_TASK_WRITE:
		retval=IBV_WR_RDMA_WRITE;
		break;
	default:
		retval=IBV_WR_SEND;
		break;
	}
		
	return retval;
}

