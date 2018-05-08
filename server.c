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
	msg->nents=1;
	msg->data=(struct hmr_iovec*)calloc(1,sizeof(struct hmr_iovec));
	if(!msg->data){
		ERROR_LOG("allocate memory error.");
		return ;
	}
	msg->msg_type=HMR_MSG_NORMAL;
	msg->data->base=strdup("hello wolrd server.");
	msg->data->length=strlen(msg->data->base)+1;
	msg->data->next=NULL;
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
		hmr_rdma_send(accept_rdma_trans, msg);
		msg.msg_type=HMR_MSG_NORMAL;
		msg.data->base=strdup("hello wolrd server copy.");
		msg.data->length=strlen(msg.data->base)+1;
		hmr_rdma_send(accept_rdma_trans, msg);
	}
	pthread_join(ctx->epoll_pthread,NULL);

	hmr_rdma_release();
	return 0;
}

