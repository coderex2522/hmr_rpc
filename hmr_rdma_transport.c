#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <linux/list.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "hmr_base.h"
#include "hmr_epoll.h"
#include "hmr_context.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"

static int rdma_num_devices=0;
LIST_HEAD(dev_list);

static struct hmr_device *hmr_rdma_dev_init(struct ibv_context *verbs)
{
	struct hmr_device *dev;

	dev=(struct hmr_device*)calloc(1,sizeof(struct hmr_device));
	if(!dev){
		ERROR_LOG("Allocate hmr_device error.");
		goto exit;
	}

	dev->pd=ibv_alloc_pd(verbs);
	if(!dev->pd){
		ERROR_LOG("Allocate ibv_pd error.");
		goto cleandev;
	}

	dev->verbs=verbs;
	INIT_LIST_HEAD(&dev->dev_list_entry);

	return dev;
	
cleandev:
	free(dev);
	dev=NULL;
	
exit:
	return dev;
}


static int hmr_rdma_transport_init()
{
	struct ibv_context **ctx_list;
	struct hmr_device *dev;
	int i,num_devices=0;
	
	ctx_list=rdma_get_devices(&num_devices);
	if(!ctx_list)
	{
		ERROR_LOG("Failed to get the rdma device list.");
		return -1;
	}
	
	rdma_num_devices=num_devices;

	for(i=0;i<num_devices;i++){
		if(!ctx_list[i]){
			ERROR_LOG("RDMA device [%d] is NULL.",i);
			--rdma_num_devices;
			continue;
		}
		dev=hmr_rdma_dev_init(ctx_list[i]);
		if(!dev){
			ERROR_LOG("RDMA device [%d]: name= %s allocate error.",i,ibv_get_device_name(ctx_list[i]->device));
			continue;
		}
		else
			INFO_LOG("RDMA device [%d]: name= %s allocate success.",i,ibv_get_device_name(ctx_list[i]->device));
		list_add(&dev->dev_list_entry,&dev_list);
	}

	rdma_free_devices(ctx_list);
	return 0;
}

static int hmr_rdma_transport_release()
{
	INFO_LOG("%s.",__func__);
	return 0;
}

static int on_cm_addr_resolved(struct rdma_cm_event *event,struct hmr_rdma_transport *rdma_trans)
{
	int retval=0;
	TRACE_LOG("RDMA Event Channel resolve addr.");

	retval=rdma_resolve_route(rdma_trans->cm_id, ROUTE_RESOLVE_TIMEOUT);
	if(retval){
		ERROR_LOG("RDMA resolve route error.");
		return retval;
	}

	return retval;
}

static int on_cm_route_resolved(struct rdma_cm_event *event,struct hmr_rdma_transport *rdma_trans)
{

}
static int on_cm_connect_request(struct rdma_cm_event *event,struct hmr_rdma_transport *rdma_trans)
{

}

static int on_cm_established(struct rdma_cm_event *event,struct hmr_rdma_transport *rdma_trans)
{

}

static int on_cm_disconnected(struct rdma_cm_event *event,struct hmr_rdma_transport *rdma_trans)
{

}

static int on_cm_error(struct rdma_cm_event *event,struct hmr_rdma_transport *rdma_trans)
{
	
}

static int hmr_handle_ec_event(struct rdma_cm_event *event,struct hmr_rdma_transport *rdma_trans)
{
	int retval=0;
	INFO_LOG("cm event [%s],status:%d",rdma_event_str(event->event),event->status);
	
	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		retval=on_cm_addr_resolved(event, rdma_trans);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		retval=on_cm_route_resolved(event, rdma_trans);
		break;
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		retval=on_cm_connect_request(event, rdma_trans);
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		retval=on_cm_established(event, rdma_trans);
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
		retval=on_cm_disconnected(event, rdma_trans);
		break;
	default:
		retval=on_cm_error(event, rdma_trans);
		break;
	};
	
	return retval;
}

static void hmr_rdma_event_channel_handler(int fd,void *data)
{
	struct rdma_event_channel *ec=(struct rdma_event_channel*)data;
	struct rdma_cm_event *event,event_copy;
	struct hmr_rdma_transport *rdma_trans;
	int retval=0;

	event=NULL;
	while ((retval=rdma_get_cm_event(ec,&event))==0){
		memcpy(&event_copy,event,sizeof(*event));
		rdma_ack_cm_event(event);

		rdma_trans=(struct hmr_rdma_transport*)event->id->context;
		if(hmr_handle_ec_event(&event_copy,rdma_trans))
			break;
	}
	
	if(retval)
		ERROR_LOG("RDMA get cm event error.");
}

static int hmr_init_rdma_event_channel(struct hmr_rdma_transport *rdma_trans)
{
	int flags,retval=0;
	
	rdma_trans->event_channel=rdma_create_event_channel();
	if(!rdma_trans->event_channel){
		ERROR_LOG("RDMA create event channel error.");
		return -1;
	}

	flags=fcntl(rdma_trans->event_channel->fd,F_GETFL,0);
	if(flags!=-1)
		flags=fcntl(rdma_trans->event_channel->fd,F_SETFL,flags|O_NONBLOCK);

	if(flags==-1){
		retval=-1;
		ERROR_LOG("Set RDMA event channel nonblock error.");
		goto cleanec;
	}

	hmr_context_add_event_fd(rdma_trans->ctx, rdma_trans->event_channel->fd,
						EPOLLIN,hmr_rdma_event_channel_handler, 
						rdma_trans->event_channel);
cleanec:
	rdma_destroy_event_channel(rdma_trans->event_channel);
	return retval;
}

static struct hmr_rdma_transport *hmr_rdma_transport_create(struct hmr_context *ctx)
{
	struct hmr_rdma_transport *rdma_trans;

	rdma_trans=(struct hmr_rdma_transport*)calloc(1,sizeof(struct hmr_rdma_transport));
	if(!rdma_trans){
		ERROR_LOG("Allocate hmr_rdma_transport memory error.",__func__);
		return NULL;
	}
	rdma_trans->ctx=ctx;
	hmr_init_rdma_event_channel(rdma_trans);
	
	return rdma_trans;
}

static int hmr_init_port_uri(struct hmr_rdma_transport *rdma_trans,
						const char *url,const char *port)
{
	struct sockaddr_in peer_addr;
	int iport=0,retval=0;

	iport=atoi(port);
	memset(&peer_addr,0,sizeof(peer_addr));
	peer_addr.sin_family=AF_INET;//PF_INET=AF_INET
	peer_addr.sin_port=htons(iport);

	retval=inet_pton(AF_INET,url,&peer_addr.sin_addr);
	if(retval<=0){
		ERROR_LOG("IP Transfer Error.");
		goto exit;
	}
	memcpy(&rdma_trans->peer_addr,&peer_addr,sizeof(struct sockaddr_in));
exit:
	return retval;
}

static int hmr_rdma_transport_connect(struct hmr_rdma_transport* rdma_trans,
								const char *url,const char*port)
{
	int retval=0;
	if(!url||!port){
		ERROR_LOG("Url or port input error.");
		return -1;
	}

	retval=hmr_init_port_uri(rdma_trans, url, port);
	if(retval<0){
		ERROR_LOG("rdma init port uri error.");
		return retval;
	}
	
	retval=rdma_create_id(rdma_trans->event_channel,
					&rdma_trans->cm_id,rdma_trans,RDMA_PS_TCP);
	if(retval==-1){
		ERROR_LOG("Rdma create id error.");
		goto cleanrdmatrans;
	}

	retval=rdma_resolve_addr(rdma_trans->cm_id,NULL,
					(struct sockaddr*)&rdma_trans->peer_addr,
					ADDR_RESOLVE_TIMEOUT);
	if(retval){
		ERROR_LOG("RDMA Device resolve addr error.");
		goto cleancmid;
	}

	return retval;
	
cleancmid:
	rdma_destroy_id(rdma_trans->cm_id);

cleanrdmatrans:
	rdma_trans->cm_id=NULL;
	
	return retval;
}

struct hmr_rdma_transport_operations rdma_trans_ops={
	.init		= hmr_rdma_transport_init,
	.release	= hmr_rdma_transport_release,
	.create		= hmr_rdma_transport_create,
	.connect	= hmr_rdma_transport_connect
};	
