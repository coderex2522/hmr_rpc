#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_mem.h"


struct hmr_mempool *hmr_mempool_create(struct hmr_rdma_transport *rdma_trans, int is_nvm)
{
	struct hmr_mempool *mempool;
	int err=0;
	
	mempool=(struct hmr_mempool*)calloc(1,sizeof(mempool));
	if(!mempool){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	/*
	if(is_nvm)
		mempool->send_base=nvm_malloc(ALLOC_MEM_SIZE*2);
	else*/
	mempool->send_base=malloc(ALLOC_MEM_SIZE*2);
	if(!mempool->send_base){
		 ERROR_LOG("allocate memory error.");
		 goto cleanmempool;
	}
	mempool->recv_base=mempool->send_base+ALLOC_MEM_SIZE;

	mempool->send_mr=ibv_reg_mr(rdma_trans->device->pd, mempool->send_base, 
							ALLOC_MEM_SIZE,
							IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | 
							IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);

	if(!mempool->send_mr){
		ERROR_LOG("RDMA register memory error.");
		goto cleansendbase;
	}

	mempool->recv_mr=ibv_reg_mr(rdma_trans->device->pd, mempool->recv_base, 
							ALLOC_MEM_SIZE,
							IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | 
							IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
	if(!mempool->recv_mr){
		ERROR_LOG("RDMA register memory error.");
		goto cleansendmr;
	}

	return mempool;
	
cleansendmr:

	ibv_dereg_mr(mempool->send_mr);
	
cleansendbase:
	free(mempool->send_base);
	
cleanmempool:
	free(mempool);
	
	return NULL;
}


void hmr_mempool_release(struct hmr_mempool *mempool)
{
	if(!mempool){
		ERROR_LOG("mempool is NULL.not need to release");
		return ;
	}
	ibv_dereg_mr(mempool->send_mr);
	ibv_dereg_mr(mempool->recv_mr);

	free(mempool->send_base);
	free(mempool->recv_base);
}

