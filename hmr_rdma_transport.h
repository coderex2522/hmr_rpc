#ifndef HMR_RDMA_TRANS_H
#define HMR_RDMA_TRANS_H

#define ADDR_RESOLVE_TIMEOUT 500
#define ROUTE_RESOLVE_TIMEOUT 500

/*call ibv_create_cq alloc size--second parameter*/
#define CQE_ALLOC_SIZE 20

/*set the max send/recv wr num*/
#define MAX_SEND_WR 256
#define MAX_RECV_WR 256

/*set the memory max size*/
#define MAX_MEM_SIZE 4096

extern struct list_head dev_list;

enum hmr_rdma_transport_state {
	HMR_RDMA_TRANSPORT_STATE_INIT,
	HMR_RDMA_TRANSPORT_STATE_LISTEN,
	HMR_RDMA_TRANSPORT_STATE_CONNECTING,
	HMR_RDMA_TRANSPORT_STATE_CONNECTED,
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

	struct ibv_mr *send_mr;
	struct ibv_mr *recv_mr;

	char *send_region;
	char *recv_region;

	int is_client;
	//struct hmr_rdma_transport_operations *ops;
	struct hmr_mempool *normal_mempool;
#ifdef HMR_NVM_ENABLE
	struct hmr_mempool *nvm_mempool;
	struct hmr_mempool *nvm_buffer;
#endif

	struct hmr_rdma_transport *accept_rdma_trans;
};

struct hmr_rdma_transport_operations{
	int		(*init)();
	int		(*release)();
	struct hmr_rdma_transport*	(*create)(struct hmr_context *ctx);
	int		(*connect)(struct hmr_rdma_transport* rdma_trans,
								const char *url,const char*port);
	int		(*listen)(struct hmr_rdma_transport* rdma_trans);
	struct hmr_rdma_transport	*(*accept)(struct hmr_rdma_transport *rdma_trans);
};


#endif