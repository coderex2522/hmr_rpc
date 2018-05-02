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
	struct hmr_rdma_transport *listen_rdma_trans,*new_rdma_trans;
	
	ctx=hmr_context_create();
	rdma_trans_ops.init();
	listen_rdma_trans=rdma_trans_ops.create(ctx);
	rdma_trans_ops.listen(listen_rdma_trans);

	while((new_rdma_trans=rdma_trans_ops.accept(listen_rdma_trans))!=NULL){
		rdma_trans_ops.send(new_rdma_trans);
	}
	
	return 0;
}

