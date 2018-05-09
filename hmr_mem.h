#ifndef HMR_MEM_H
#define HMR_MEM_H

#define ALLOC_MEM_SIZE 1024

struct hmr_mempool{
	void *send_region;
	void *recv_region;
	void *rdma_region;
	
	struct ibv_mr *send_mr;
	struct ibv_mr *recv_mr;
	struct ibv_mr *rdma_mr;

	/*used for buffer control*/
	int used_send_region;
	int used_recv_region;
	int used_rdma_region;
};

struct hmr_mempool *hmr_mempool_create(struct hmr_rdma_transport *rdma_trans, int is_nvm);
void hmr_mempool_release(struct hmr_mempool *mempool);

void *hmr_mempool_alloc_addr(struct hmr_mempool *mempool, int length, int is_send);
#endif

