#ifndef HMR_TASK_H
#define HMR_TASK_H

struct hmr_sge{
	void		*addr;
	int 		length;
	uint32_t		lkey;
};

enum hmr_task_type{
	/*HMR_TASK_MR only use for exchange the two side remote_mr info*/
	HMR_TASK_MR,
	HMR_TASK_NORMAL,
	HMR_TASK_READ,
	HMR_TASK_WRITE,
	HMR_TASK_FINISH,
	HMR_TASK_DONE
};
	
struct hmr_task{
	enum hmr_task_type task_type;
	struct hmr_sge sge_list;
	struct hmr_rdma_transport *rdma_trans;
	struct list_head task_list_entry;
};

struct hmr_task *hmr_send_task_create(struct hmr_rdma_transport *rdma_trans, struct hmr_msg *msg);
struct hmr_task *hmr_recv_task_create(struct hmr_rdma_transport *rdma_trans, int size);

#endif
