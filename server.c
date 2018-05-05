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
	struct hmr_rdma_transport *accept_rdma_trans;
	int err=0;
	
	ctx=hmr_context_create();
	rdma_trans_ops.init();
	rdma_trans=rdma_trans_ops.create(ctx);
	rdma_trans_ops.listen(rdma_trans);
	err=pthread_create(&ctx->epoll_pthread,NULL,hmr_context_run,ctx);
	if(err){
		ERROR_LOG("pthread create error.");
		return -1;
	}

	while((accept_rdma_trans=rdma_trans_ops.accept(rdma_trans))!=NULL){
		INFO_LOG("accept success.");
	}
	pthread_join(ctx->epoll_pthread,NULL);
	return 0;
}

