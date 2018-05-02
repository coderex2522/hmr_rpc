#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_mem.h"

const int order_size[HMR_ORDER_SIZE]={4,8,16,32,64,128,256,512,1024,2048,4096};

struct hmr_mempool *hmr_mempool_create()
{
	struct hmr_mempool *mempool;

	mempool=(struct hmr_mempool*)calloc(1,sizeof(struct hmr_mempool));
	if(!mempool){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	for(i=0;i<HMR_MR_LIST_SIZE;i++){

		
	}
}
struct hmr_iovec *alloc_iovec_mem(struct hmr_rdma_transport *rdma_trans,int size,int msg_flags)
{
	struct hmr_iovec_mem *iovec_mem;
	int index=0;

	iovec_mem=(struct hmr_iovec_mem*)calloc(1,sizeof(iovec_mem));
	if(!iovec_mem){
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	if(size<=0||size>4096){
		ERROR_LOG("alloc size must not less than zero and less than 4096.");
		goto cleanmem;
	}
	
	for(index=0;index<HMR_ORDER_SIZE;index++){
		if(size<=order_size[index])
			break;
	}

	if(list_empty(&rdma_trans->normal_mempool->empty_mr_list[index])){
		iovec_mem->iovec.iov_base=malloc(order_size[index]);
		iovec_mem->length=order_size[index];
		iovec_mem->mr=ibv_reg_mr(rdma_trans->device->pd, 
					iovec_mem->iovec.iov_base, 
					order_size[index],
					IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_WRITE);
		INIT_LIST_HEAD(iovec_mem->iovec_list_entry);
	}
	else{
		iovec_mem=container_of(&(rdma_trans->normal_mempool->empty_mr_list[index]), 
							struct hmr_iovec_mem, 
							iovec_list_entry);
		list_del(&iovec_mem->iovec_list_entry);
	}

	return &iovec_mem->iovec;
	
cleanmem:
	free(iovec_mem);
	return NULL;
}

 
