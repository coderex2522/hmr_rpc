#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_task.h"
#include "hmr_utils.h"
#include "hmr_timerfd.h"


int hmr_timerfd_create(struct itimerspec *new_value)
{
	int tfd,err=0;

	tfd=timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if(tfd<0){
		ERROR_LOG("create send task timerfd error.");
		return -1;
	}

	err=timerfd_settime(tfd, 0, new_value, NULL);
	if(err){
		ERROR_LOG("timerfd settime error.");
		return -1;
	}

	return tfd;
}


void hmr_send_task_handler(int fd,void *data)
{
	struct hmr_rdma_transport *rdma_trans=(struct hmr_rdma_transport *)data;
	struct ibv_send_wr send_wr[MAX_SEND_WR+1],*sw_head,*bad_wr=NULL;
	struct ibv_sge sge[MAX_SEND_WR+1],*sge_head;
	struct hmr_task *send_task=NULL,*st_next;
	int err=0;
	
	uint64_t exp=0;
	read(fd, &exp, sizeof(uint64_t));
	INFO_LOG("%d send task success.",fd);

	if(list_empty(&rdma_trans->send_task_list))
		return ;
	
	sw_head=&send_wr[0];
	sge_head=&sge[0];
	list_for_each_entry(send_task, &rdma_trans->send_task_list, task_list_entry){
		memset(sw_head, 0, sizeof(struct ibv_send_wr));
		sw_head->wr_id=(uintptr_t)send_task;
		sw_head->num_sge=1;
		sw_head->opcode=hmr_get_opcode_from_task(send_task);
		sw_head->sg_list=sge_head;
		sw_head->send_flags=IBV_SEND_SIGNALED;
		sw_head->next=sw_head+1;

		sge_head->addr=(uintptr_t)send_task->sge_list.addr;
		sge_head->length=send_task->sge_list.length;
		sge_head->lkey=send_task->sge_list.lkey;

		if(sw_head->opcode==IBV_WR_RDMA_WRITE){
			sw_head->wr.rdma.remote_addr=(uintptr_t)(rdma_trans->peer_info.normal_mr.addr+rdma_trans->peer_info.used_normal_size);
			sw_head->wr.rdma.rkey=rdma_trans->peer_info.normal_mr.rkey;
			rdma_trans->peer_info.used_normal_size+=sge_head->length;
		}
		sw_head++;
		sge_head++;
	}
	sw_head--;
	sw_head->next=NULL;
	
	/*

	if(send_wr.opcode==IBV_WR_RDMA_READ||send_wr.opcode==IBV_WR_RDMA_WRITE){
		INFO_LOG("%s post rdma write. %p",__func__,rdma_trans->peer_info.normal_mr.addr);
		send_wr.wr.rdma.remote_addr=(uintptr_t)(rdma_trans->peer_info.normal_mr.addr);
		send_wr.wr.rdma.rkey=rdma_trans->peer_info.normal_mr.rkey;
	}*/
	
	err=ibv_post_send(rdma_trans->qp, &send_wr[0], &bad_wr);
	if(err){
		ERROR_LOG("ibv post send error.");
	}
	else{
		/* delete send task from the send task list*/
		list_for_each_entry_safe(send_task, st_next, &rdma_trans->send_task_list, task_list_entry) {
			list_del_init(&send_task->task_list_entry);
			/*free send task in the wc success handler.*/
		}
	}
	INFO_LOG("send task success.");
}


void hmr_sync_nvm_handler(int fd,void *data)
{
	//struct hmr_rdma_transport *rdma_trans=(struct hmr_rdma_transport *)data;
	uint64_t exp=0;
	read(fd, &exp, sizeof(uint64_t));
	INFO_LOG("%d sync nvm success.",fd);
}

