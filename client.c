#include <stdio.h>
#include <stdlib.h>
#include <linux/list.h>
#include <netinet/in.h>

#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_epoll.h"
#include "hmr_context.h"

extern struct hmr_rdma_transport_operations rdma_trans_ops;

int main(int argc,char **argv)
{
	struct hmr_context *ctx;
	struct hmr_rdma_transport *rdma_trans;
	
	ctx=hmr_context_create();
	rdma_trans_ops.init();
	rdma_trans=rdma_trans_ops.create(ctx);
	rdma_trans_ops.connect(rdma_trans,argv[1],argv[2]);
	hmr_context_listen_fd(ctx);
	return 0;
}

