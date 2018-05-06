#ifndef HMR_MEM_H
#define HMR_MEM_H

#define ALLOC_MEM_SIZE 1024

struct hmr_mempool{
	char *send_region;
	char *recv_region;

	struct ibv_mr *send_mr;
	struct ibv_mr *recv_mr;
};

struct hmr_mempool *hmr_mempool_create(struct hmr_rdma_transport *rdma_trans, int is_nvm);
void hmr_mempool_release(struct hmr_mempool *mempool);

#endif

