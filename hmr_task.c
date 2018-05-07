#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_mem.h"
#include "hmr_task.h"

struct hmr_task *hmr_send_task_create(struct hmr_rdma_transport *rdma_trans, struct hmr_msg *msg)
{
	struct hmr_task *task;
	struct hmr_iovec *data_head;
	int i=0;
	task=(struct hmr_task*)calloc(1,sizeof(struct hmr_task));
	if(!task){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	task->rdma_trans=rdma_trans;
	task->sge_list=(struct hmr_sge*)calloc(msg->nents, sizeof(struct hmr_sge));
	if(!task->sge_list){
		ERROR_LOG("allocate memory error.");
		goto cleantask;
	}

	data_head=msg->data;
	for(i=0;data_head!=NULL&&i<msg->nents;i++){
		task->sge_list[i].length=data_head->length+1;
		task->sge_list[i].addr=hmr_mempool_alloc_addr(rdma_trans->normal_mempool, data_head->length+1, 1);
		task->sge_list[i].lkey=rdma_trans->normal_mempool->send_mr->lkey;
		memcpy(task->sge_list[i].addr,data_head->base,data_head->length+1);
		data_head=data_head->next;
		task->nents++;
	}
	
	INIT_LIST_HEAD(&task->task_list_entry);
	return task;

cleantask:
	free(task);
	return NULL;
}

struct hmr_task *hmr_recv_task_create(struct hmr_rdma_transport *rdma_trans)
{
	struct hmr_task *task;
	
	task=(struct hmr_task*)calloc(1,sizeof(struct hmr_task));
	if(!task){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	task->sge_list=(struct hmr_sge*)calloc(1, sizeof(struct hmr_sge));
	if(!task->sge_list){
		ERROR_LOG("allocate memory error.");
		goto cleantask;
	}
	task->rdma_trans=rdma_trans;
	task->sge_list[0].length=MAX_RECV_SIZE;
	task->sge_list[0].addr=hmr_mempool_alloc_addr(rdma_trans->normal_mempool, MAX_RECV_SIZE, 0);
	task->sge_list[0].lkey=rdma_trans->normal_mempool->recv_mr->lkey;
	task->nents++;
	
	INIT_LIST_HEAD(&task->task_list_entry);
	return task;

cleantask:
	free(task);
	return NULL;
}


