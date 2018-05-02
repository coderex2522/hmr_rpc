#ifndef HMR_MEM_H
#define HMR_MEM_H

#define HMR_MR_LIST_SIZE 11
#define HMR_ORDER_SIZE HMR_MR_LIST_SIZE

struct hmr_iovec_mem{
	struct hmr_iovec iovec;
	/*the length is real size in memory allocator */
	int length;
	struct ibv_mr *mr;
	struct list_head iovec_list_entry;
};

/*base_addr:8 16 32 64 128 256 512 1024 2048*/
struct hmr_mempool{
	struct list_head empty_mr_list[HMR_MR_LIST_SIZE];
	struct list_head used_mr_list[HMR_MR_LIST_SIZE];
};

struct hmr_iovec *alloc_iovec_mem(struct hmr_mempool *mempool, int size);

/*note:need to consider the DRAM buffer for NVM*/
/*
struct hmr_nvm_mempool{

};


int alloc_iovec_nvmmem(struct hmr_mempool * mempool, struct hmr_iovec * iovec);
*/
#endif
