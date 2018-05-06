#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_mem.h"


struct hmr_mempool *hmr_mempool_create(struct hmr_rdma_transport *rdma_trans, int is_nvm)
{
	struct hmr_mempool *mempool;
	int err=0;
	
	mempool=(struct hmr_mempool*)calloc(1,sizeof(struct hmr_mempool));
	if(!mempool){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	mempool->send_region=malloc(ALLOC_MEM_SIZE*2);
	if(!mempool->send_region){
		 ERROR_LOG("allocate memory error.");
		 goto cleanmempool;
	}
	mempool->recv_region=mempool->send_region+ALLOC_MEM_SIZE;

	mempool->send_mr=ibv_reg_mr(rdma_trans->device->pd, mempool->send_region, 
							ALLOC_MEM_SIZE,
							IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | 
							IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);

	if(!mempool->send_mr){
		ERROR_LOG("RDMA register memory error.");
		goto cleansendregion;
	}

	mempool->recv_mr=ibv_reg_mr(rdma_trans->device->pd, mempool->recv_region, 
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
	
cleansendregion:
	free(mempool->send_region);
	
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

	free(mempool->send_region);
	//free(mempool->recv_region);
}

