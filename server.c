#include <stdio.h>
#include <stdlib.h>
#include <linux/list.h>
#include <netinet/in.h>

#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_epoll.h"
#include "hmr_context.h"


void build_msg(struct hmr_msg *msg)
{
	msg->msg_type=HMR_MSG_NORMAL;
	msg->data=strdup("hello wolrd server.");
	msg->data_size=strlen(msg->data)+1;
}

void process_response(struct hmr_msg * msg)
{
	INFO_LOG("recv content %s",msg->data);
}

int main(int argc,char **argv)
{
	struct hmr_context *ctx;
	struct hmr_rdma_transport *rdma_trans;
	struct hmr_rdma_transport *accept_rdma_trans;
	struct hmr_msg msg;
	int err=0;

	build_msg(&msg);
	
	hmr_rdma_init();
	
	ctx=hmr_context_create();
	rdma_trans=hmr_rdma_create(ctx);
	hmr_rdma_listen(rdma_trans);
	err=pthread_create(&ctx->epoll_pthread,NULL,hmr_context_run,ctx);
	if(err){
		ERROR_LOG("pthread create error.");
		return -1;
	}

	while((accept_rdma_trans=hmr_rdma_accept(rdma_trans))!=NULL){
		INFO_LOG("accept success.");
		hmr_rdma_send(accept_rdma_trans, &msg);
	}
	pthread_join(ctx->epoll_pthread,NULL);

	hmr_rdma_release();
	return 0;
}

