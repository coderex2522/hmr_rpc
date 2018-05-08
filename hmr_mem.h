#ifndef HMR_MEM_H
#define HMR_MEM_H

#define ALLOC_MEM_SIZE 1024

struct hmr_mempool{
	void *send_region;
	void *recv_region;
	
	int sr_used_size;
	int rr_used_size;
	
	struct ibv_mr *send_mr;
	struct ibv_mr *recv_mr;
	//int start_recv_region;
	//int end_recv_region;
};

struct hmr_mempool *hmr_mempool_create(struct hmr_rdma_transport *rdma_trans, int is_nvm);
void hmr_mempool_release(struct hmr_mempool *mempool);

void *hmr_mempool_alloc_addr(struct hmr_mempool *mempool, int length, int is_send);
#endif

