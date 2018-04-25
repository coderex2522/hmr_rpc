#ifndef HMR_RDMA_TRANS_H
#define HMR_RDMA_TRANS_H

#define ADDR_RESOLVE_TIMEOUT 500
#define ROUTE_RESOLVE_TIMEOUT 500

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
};

struct hmr_cq{
	struct ibv_cq	*cq;
	struct ibv_comp_channel	*channel;
	struct hmr_device *device;
};

struct hmr_rdma_transport{
	struct sockaddr_in	peer_addr;
	struct sockaddr_in local_addr;

	struct hmr_device *device;
	struct hmr_cq	*hcq;
	struct ibv_qp	*qp;
	struct rdma_event_channel	*event_channel;
	struct hmr_context *ctx;
	struct rdma_cm_id	*cm_id;

	enum hmr_rdma_transport_state trans_state;
	//struct hmr_rdma_transport_operations *ops;
};

struct hmr_rdma_transport_operations{
	int		(*init)();
	int		(*release)();
	struct hmr_rdma_transport*	(*create)(struct hmr_context *ctx);
	int		(*connect)(struct hmr_rdma_transport* rdma_trans,
								const char *url,const char*port);
};


#endif