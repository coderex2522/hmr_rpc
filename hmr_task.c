#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_mem.h"
#include "hmr_task.h"

struct hmr_task *hmr_send_task_create(struct hmr_rdma_transport *rdma_trans, struct hmr_msg *msg)
{
	struct hmr_task *task;
	
	task=(struct hmr_task*)calloc(1,sizeof(struct hmr_task));
	if(!task){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	task->rdma_trans=rdma_trans;
	task->sge_list.length=sizeof(enum hmr_msg_type)+msg->data_size;
	task->sge_list.addr=hmr_mempool_alloc_addr(rdma_trans->normal_mempool, task->sge_list.length, 1);
	task->sge_list.lkey=rdma_trans->normal_mempool->send_mr->lkey;
	
	memcpy(task->sge_list.addr,&msg->msg_type,sizeof(enum hmr_msg_type));
	memcpy(task->sge_list.addr+sizeof(enum hmr_msg_type),msg->data,msg->data_size);
	
	INIT_LIST_HEAD(&task->task_list_entry);
	return task;
}

struct hmr_task *hmr_recv_task_create(struct hmr_rdma_transport *rdma_trans)
{
	struct hmr_task *task;
	
	task=(struct hmr_task*)calloc(1,sizeof(struct hmr_task));
	if(!task){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	task->rdma_trans=rdma_trans;
	task->sge_list.length=MAX_RECV_SIZE;
	task->sge_list.addr=hmr_mempool_alloc_addr(rdma_trans->normal_mempool, MAX_RECV_SIZE, 0);
	task->sge_list.lkey=rdma_trans->normal_mempool->recv_mr->lkey;
	
	INIT_LIST_HEAD(&task->task_list_entry);
	return task;
}


