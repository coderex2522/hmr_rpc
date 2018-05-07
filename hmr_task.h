#ifndef HMR_TASK_H
#define HMR_TASK_H

struct hmr_sge{
	void		*addr;
	int 		length;
	uint32_t		lkey;
};

struct hmr_task{
	struct hmr_sge *sge_list;
	int nents;
	struct hmr_rdma_transport *rdma_trans;
	struct list_head task_list_entry;
};

struct hmr_task *hmr_send_task_create(struct hmr_rdma_transport *rdma_trans, struct hmr_msg *msg);
struct hmr_task *hmr_recv_task_create(struct hmr_rdma_transport *rdma_trans);

#endif
