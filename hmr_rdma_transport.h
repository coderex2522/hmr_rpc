#ifndef HMR_RDMA_TRANS_H
#define HMR_RDMA_TRANS_H

#define ADDR_RESOLVE_TIMEOUT 500
#define ROUTE_RESOLVE_TIMEOUT 500

/*call ibv_create_cq alloc size--second parameter*/
#define CQE_ALLOC_SIZE 20

/*set the max send/recv wr num*/
#define MAX_SEND_WR 256
#define MAX_RECV_WR 256

#define MAX_SEND_SGE 4

#define MAX_RECV_SIZE 128
/*set the memory max size*/
#define MAX_MEM_SIZE 1024


#define MIN_RECV_NUM 3
#define INC_RECV_NUM 1

extern struct list_head dev_list;

enum hmr_rdma_transport_state {
	HMR_RDMA_TRANSPORT_STATE_INIT,
	HMR_RDMA_TRANSPORT_STATE_LISTEN,
	HMR_RDMA_TRANSPORT_STATE_CONNECTING,
	HMR_RDMA_TRANSPORT_STATE_CONNECTED,
	/* when the two sides exchange the info of one sided RDMA,
	 * the trans state will from CONNECTED to S-R-DCONNECTED
	 */
	HMR_RDMA_TRANSPORT_STATE_SCONNECTED,
	HMR_RDMA_TRANSPORT_STATE_RCONNECTED,
	HMR_RDMA_TRANSPORT_STATE_DISCONNECTED,
	HMR_RDMA_TRANSPORT_STATE_RECONNECT,
	HMR_RDMA_TRANSPORT_STATE_CLOSED,
	HMR_RDMA_TRANSPORT_STATE_DESTROYED,
	HMR_RDMA_TRANSPORT_STATE_ERROR
};
	
struct hmr_device{
	struct list_head dev_list_entry;
	struct ibv_context	*verbs;
	struct ibv_pd	*pd;
	struct ibv_device_attr device_attr;
};

struct hmr_cq{
	struct ibv_cq	*cq;
	struct ibv_comp_channel	*comp_channel;
	struct hmr_device *device;
	/*add the fd of comp_channel into the ctx*/
	struct hmr_context *ctx;
};

struct hmr_peer_info{
	struct ibv_mr normal_mr;
#ifdef HMR_NVM_ENABLE
	struct ibv_mr nvm_buffer_mr;
	struct ibv_mr nvm_mr;
#endif
};

struct hmr_rdma_transport{
	struct sockaddr_in	peer_addr;
	struct sockaddr_in local_addr;

	struct hmr_device *device;
	struct hmr_context *ctx;
	struct hmr_cq	*hcq;
	struct ibv_qp	*qp;
	struct rdma_event_channel	*event_channel;
	struct rdma_cm_id	*cm_id;

	enum hmr_rdma_transport_state trans_state;
	
	int is_client;
	
	struct hmr_peer_info peer_info;
	
	struct hmr_mempool *normal_mempool;
#ifdef HMR_NVM_ENABLE
	struct hmr_mempool *nvm_mempool;
	struct hmr_mempool *nvm_buffer;
#endif

	/*pre commit post recv num*/
	int cur_recv_num;
	int default_recv_size;
	
	struct list_head send_task_list;
	struct hmr_rdma_transport *accept_rdma_trans;
	void (*process_resp)(struct hmr_msg *msg);
};

void hmr_rdma_init();

void hmr_rdma_release();

struct hmr_rdma_transport *hmr_rdma_create(struct hmr_context *ctx);

int hmr_rdma_connect(struct hmr_rdma_transport* rdma_trans, const char *url, const char*port);
								
int hmr_rdma_listen(struct hmr_rdma_transport *rdma_trans);
	
struct hmr_rdma_transport *hmr_rdma_accept(struct hmr_rdma_transport *rdma_trans);

int hmr_rdma_send(struct hmr_rdma_transport *rdma_trans, struct hmr_msg *msg);

#endif