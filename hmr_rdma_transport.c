#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "hmr_base.h"
#include "hmr_epoll.h"
#include "hmr_context.h"
#include "hmr_log.h"
#include "hmr_rdma_transport.h"
#include "hmr_mem.h"

static int rdma_num_devices=0;
static int test_num=0;
LIST_HEAD(dev_list);

static struct hmr_rdma_transport *hmr_rdma_transport_create(struct hmr_context *ctx);
static struct hmr_rdma_transport *hmr_rdma_transport_accept(struct hmr_rdma_transport *rdma_trans);


static struct hmr_device *hmr_rdma_dev_init(struct ibv_context *verbs)
{
	struct hmr_device *dev;
	int retval=0;
	INFO_LOG("HMR Device ibv context %p",verbs);
	dev=(struct hmr_device*)calloc(1,sizeof(struct hmr_device));
	if(!dev){
		ERROR_LOG("Allocate hmr_device error.");
		goto exit;
	}

	retval=ibv_query_device(verbs, &dev->device_attr);
	if(retval<0){
		ERROR_LOG("rdma query device attr error.");
		goto cleandev;
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
	return 0;
}

static struct hmr_device *hmr_device_lookup(struct ibv_context *verbs)
{
	struct hmr_device *device;

	list_for_each_entry(device, &dev_list, dev_list_entry){
		if(device->verbs==verbs){
			return device;
		}
	}

	return NULL;
}

static int on_cm_addr_resolved(struct rdma_cm_event *event, struct hmr_rdma_transport *rdma_trans)
{
	int retval=0;

	rdma_trans->device=hmr_device_lookup(rdma_trans->cm_id->verbs);
	if(!rdma_trans->device){
		ERROR_LOG("not found the hmr device.");
		return -1;
	}

	INFO_LOG("RDMA Register Memory");
	rdma_trans->normal_mempool=hmr_mempool_create(rdma_trans,0);
#ifdef HMR_NVM_ENABLE
	rdma_trans->nvm_buffer=hmr_mempool_create(rdma_trans,0);
	rdma_trans->nvm_mempool=hmr_mempool_create(rdma_trans,1);
#endif 

	retval=rdma_resolve_route(rdma_trans->cm_id, ROUTE_RESOLVE_TIMEOUT);
	if(retval){
		ERROR_LOG("RDMA resolve route error.");
		return retval;
	}

	return retval;
}

/*---------------------------------------------------------------------------*/
/* ibv_wc_opcode_str	                                                     */
/*---------------------------------------------------------------------------*/
const char *ibv_wc_opcode_str(enum ibv_wc_opcode opcode)
{
	switch (opcode) {
	case IBV_WC_SEND:		return "IBV_WC_SEND";
	case IBV_WC_RDMA_WRITE:		return "IBV_WC_RDMA_WRITE";
	case IBV_WC_RDMA_READ:		return "IBV_WC_RDMA_READ";
	case IBV_WC_COMP_SWAP:		return "IBV_WC_COMP_SWAP";
	case IBV_WC_FETCH_ADD:		return "IBV_WC_FETCH_ADD";
	case IBV_WC_BIND_MW:		return "IBV_WC_BIND_MW";
	/* recv-side: inbound completion */
	case IBV_WC_RECV:		return "IBV_WC_RECV";
	case IBV_WC_RECV_RDMA_WITH_IMM: return "IBV_WC_RECV_RDMA_WITH_IMM";
	default:			return "IBV_WC_UNKNOWN";
	};
}


static void hmr_cq_comp_channel_handler(int fd, void *data)
{
	struct hmr_cq *hcq=(struct hmr_cq*)data;
	struct ibv_cq *cq;
	void *cq_context;
	struct ibv_wc wc;
	struct hmr_rdma_transport *rdma_trans;
	int err=0;

	err=ibv_get_cq_event(hcq->comp_channel, &cq, &cq_context);
	if(err){
		ERROR_LOG("ibv get cq event error.");
		return ;
	}

	ibv_ack_cq_events(hcq->cq, 1);
	err=ibv_req_notify_cq(hcq->cq, 0);
	if(err){
		ERROR_LOG("ibv req notify cq error.");
		return ;
	}
	
	while(ibv_poll_cq(hcq->cq,1,&wc)){
		if(wc.status==IBV_WC_SUCCESS){
			rdma_trans=(struct hmr_rdma_transport*)(uintptr_t)wc.wr_id;
			switch (wc.opcode)
			{
			case IBV_WC_SEND:
				INFO_LOG("IBV_WC_SEND send the content [%s] success.",rdma_trans->normal_mempool->send_region);
				test_num++;
				break;
			case IBV_WC_RECV:
				INFO_LOG("IBV_WC_RECV recv the content [%s] success.",rdma_trans->normal_mempool->recv_region);
				test_num++;
				break;
			case IBV_WC_RDMA_WRITE:
				break;
			case IBV_WC_RDMA_READ:
				break;
			default:
				ERROR_LOG("unknown opcode:%s",ibv_wc_opcode_str(wc.opcode));
				break;
			}
			if(test_num==2&&rdma_trans->is_client)
				rdma_disconnect(rdma_trans->cm_id);
		}
		else{
			ERROR_LOG("wc status [%s] is error.",ibv_wc_status_str(wc.status));
		}
	}
	
}

static struct hmr_cq* hmr_cq_get(struct hmr_device *device,struct hmr_context *ctx)
{
	struct hmr_cq *hcq;
	int retval,alloc_size,flags=0;

	hcq=(struct hmr_cq*)calloc(1,sizeof(struct hmr_cq));
	if(!hcq){
		ERROR_LOG("allocate the memory of struct hmr_cq error.");
		return NULL;
	}
	
	hcq->comp_channel=ibv_create_comp_channel(device->verbs);
	if(!hcq->comp_channel){
		ERROR_LOG("rdma device %p create comp channel error.",device);
		goto cleanhcq;
	}

	flags=fcntl(hcq->comp_channel->fd,F_GETFL,0);
	if(flags!=-1)
		flags=fcntl(hcq->comp_channel->fd,F_SETFL,flags|O_NONBLOCK);

	if(flags==-1){
		ERROR_LOG("set hcq comp channel fd nonblock error.");
		goto cleanchannel;
	}

	hcq->ctx=ctx;
	retval=hmr_context_add_event_fd(hcq->ctx, hcq->comp_channel->fd,
						EPOLLIN, hmr_cq_comp_channel_handler, hcq);
	if(retval){
		ERROR_LOG("context add comp channel fd error.");
		goto cleanchannel;		
	}

	alloc_size=min(CQE_ALLOC_SIZE,device->device_attr.max_cqe);
	hcq->cq=ibv_create_cq(device->verbs, alloc_size, hcq, hcq->comp_channel, 0);
	if(!hcq->cq){
		ERROR_LOG("ibv create cq error.");
		goto cleaneventfd;
	}

	retval=ibv_req_notify_cq(hcq->cq, 0);
	if(retval){
		ERROR_LOG("ibv req notify cq error.");
		goto cleaneventfd;
	}

	hcq->device=device;
	return hcq;
	
cleaneventfd:
	hmr_context_del_event_fd(ctx, hcq->comp_channel->fd);
	
cleanchannel:
	ibv_destroy_comp_channel(hcq->comp_channel);
	
cleanhcq:
	free(hcq);
	hcq=NULL;
	
	return hcq;
}


/**
 * @param[in] rdma_trans
 * 
 * @return 0 on success, or nozero on error.
 */
static int hmr_qp_create(struct hmr_rdma_transport *rdma_trans)
{
	int retval=0;
	struct ibv_qp_init_attr qp_init_attr;
	struct hmr_cq *hcq;
	
	hcq=hmr_cq_get(rdma_trans->device, rdma_trans->ctx);
	if(!hcq){
		ERROR_LOG("hmr cq get error.");
		return -1;
	}
	
	memset(&qp_init_attr,0,sizeof(qp_init_attr));
	qp_init_attr.qp_context=rdma_trans;
	qp_init_attr.qp_type=IBV_QPT_RC;
	qp_init_attr.send_cq=hcq->cq;
	qp_init_attr.recv_cq=hcq->cq;

	qp_init_attr.cap.max_send_wr=MAX_SEND_WR;
	qp_init_attr.cap.max_send_sge=min(rdma_trans->device->device_attr.max_sge,4);
	
	qp_init_attr.cap.max_recv_wr=MAX_RECV_WR;
	qp_init_attr.cap.max_recv_sge=1;

	retval=rdma_create_qp(rdma_trans->cm_id,rdma_trans->device->pd,&qp_init_attr);
	if(retval){
		ERROR_LOG("rdma create qp error.");
		goto cleanhcq;
	}
	
	rdma_trans->qp=rdma_trans->cm_id->qp;
	rdma_trans->hcq=hcq;
	
	return retval;
/*if provide for hcq with per context,there not delete hcq.*/
cleanhcq:
	free(hcq);
	return retval;
}

static void hmr_qp_release(struct hmr_rdma_transport* rdma_trans)
{
	if(rdma_trans->qp){
		ibv_destroy_qp(rdma_trans->qp);
		ibv_destroy_cq(rdma_trans->hcq->cq);
		hmr_context_del_event_fd(rdma_trans->ctx, rdma_trans->hcq->comp_channel->fd);
		free(rdma_trans->hcq);
		rdma_trans->hcq=NULL;
	}
}

static void hmr_post_recv(struct hmr_rdma_transport *rdma_trans)
{
	struct ibv_recv_wr recv_wr,*bad_wr=NULL;
	int err=0;
	struct ibv_sge sge;

	/*when call the ibv_poll_cq,we can get the context from wr_id*/
	recv_wr.wr_id=(uintptr_t)rdma_trans;
	recv_wr.next=NULL;
	recv_wr.sg_list=&sge;
	recv_wr.num_sge=1;

	sge.addr=(uintptr_t)rdma_trans->normal_mempool->recv_region;
	sge.length=MAX_MEM_SIZE;
	sge.lkey=rdma_trans->normal_mempool->recv_mr->lkey;

	err=ibv_post_recv(rdma_trans->qp, &recv_wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv post recv error.");
	}
}
/**
 * @param[in]
 * 
 * @return 0 on success, other on error.
 */
static int on_cm_route_resolved(struct rdma_cm_event *event, struct hmr_rdma_transport *rdma_trans)
{
	struct rdma_conn_param conn_param;
	int retval=0;
	
	retval=hmr_qp_create(rdma_trans);
	if(retval){
		ERROR_LOG("hmr qp create error.");
		return retval;
	}

	memset(&conn_param, 0, sizeof(conn_param));
	conn_param.retry_count=3;
	conn_param.rnr_retry_count=3;

	conn_param.responder_resources =
		rdma_trans->device->device_attr.max_qp_rd_atom;
	conn_param.initiator_depth =
		rdma_trans->device->device_attr.max_qp_init_rd_atom;
	
	INFO_LOG("RDMA Connect.");
	
	retval=rdma_connect(rdma_trans->cm_id, &conn_param);
	if(retval){
		ERROR_LOG("rdma connect error.");
		goto cleanqp;
	}

	hmr_post_recv(rdma_trans);
	return retval;
	
cleanqp:
	hmr_qp_release(rdma_trans);
	rdma_trans->ctx->is_stop=1;
	return retval;
}

static int on_cm_connect_request(struct rdma_cm_event *event, struct hmr_rdma_transport *rdma_trans)
{
	struct rdma_conn_param conn_param;
	struct hmr_rdma_transport *accept_rdma_trans;
	int retval=0;

	INFO_LOG("event id %p rdma_trans cm_id %p event_listenid %p",event->id,rdma_trans->cm_id,event->listen_id);
	accept_rdma_trans=hmr_rdma_transport_create(rdma_trans->ctx);
	if(!accept_rdma_trans){
		ERROR_LOG("rdma trans process connect request error.");
		return -1;
	}
	accept_rdma_trans->cm_id=event->id;
	event->id->context=accept_rdma_trans;
	accept_rdma_trans->device=hmr_device_lookup(event->id->verbs);
	if(!accept_rdma_trans->device){
		ERROR_LOG("can't find the rdma device.");
		return -1;
	}
	
	retval=hmr_qp_create(accept_rdma_trans);
	if(retval){
		ERROR_LOG("hmr qp create error.");
		return retval;
	}

	/*exist concurrent error.*/
	accept_rdma_trans->accept_rdma_trans=rdma_trans->accept_rdma_trans;
	rdma_trans->accept_rdma_trans=accept_rdma_trans;
	
	return retval;
}

static void hmr_post_send_example(struct hmr_rdma_transport *rdma_trans)
{
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	int err=0;
	if(rdma_trans->is_client)
		snprintf(rdma_trans->normal_mempool->send_region,MAX_MEM_SIZE,"message from client.");
	else
		snprintf(rdma_trans->normal_mempool->send_region,MAX_MEM_SIZE,"message from server.");
	
	memset(&send_wr,0,sizeof(send_wr));

	send_wr.wr_id=(uintptr_t)rdma_trans;
	send_wr.num_sge=1;
	send_wr.opcode=IBV_WR_SEND;
	send_wr.sg_list=&sge;
	send_wr.send_flags=IBV_SEND_SIGNALED;

	sge.addr=(uintptr_t)rdma_trans->normal_mempool->send_region;
	sge.length=MAX_MEM_SIZE;
	sge.lkey=rdma_trans->normal_mempool->send_mr->lkey;

	err=ibv_post_send(rdma_trans->qp, &send_wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv post send error.");
	}
}

static int on_cm_established(struct rdma_cm_event *event, struct hmr_rdma_transport *rdma_trans)
{
	int retval=0;

	memcpy(&rdma_trans->local_addr,
		&rdma_trans->cm_id->route.addr.src_sin,
		sizeof(rdma_trans->local_addr));

	memcpy(&rdma_trans->peer_addr,
		&rdma_trans->cm_id->route.addr.dst_sin,
		sizeof(rdma_trans->peer_addr));
	
	rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_CONNECTED;

	hmr_post_send_example(rdma_trans);

	return retval;
}

static int on_cm_disconnected(struct rdma_cm_event *event, struct hmr_rdma_transport *rdma_trans)
{
	int retval=0;

	rdma_destroy_qp(rdma_trans->cm_id);

	hmr_mempool_release(rdma_trans->normal_mempool);
#ifdef HMR_NVM_ENABLE
	hmr_mempool_release(rdma_trans->nvm_buffer);
	hmr_mempool_release(rdma_trans->nvm_mempool);
#endif
	INFO_LOG("rdma disconnected success.");

	hmr_context_del_event_fd(rdma_trans->ctx, rdma_trans->hcq->comp_channel->fd);
	hmr_context_del_event_fd(rdma_trans->ctx, rdma_trans->event_channel->fd);

	if(rdma_trans->is_client)
		rdma_trans->ctx->is_stop=1;
	return retval;
}

static int hmr_handle_ec_event(struct rdma_cm_event *event)
{
	int retval=0;
	struct hmr_rdma_transport *rdma_trans;
	rdma_trans=(struct hmr_rdma_transport*)event->id->context;
		
	INFO_LOG("cm event [%s],status:%d",
			rdma_event_str(event->event),event->status);
	
	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		retval=on_cm_addr_resolved(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		retval=on_cm_route_resolved(event,rdma_trans);
		break;
	/*server can call the connect request*/
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		retval=on_cm_connect_request(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		retval=on_cm_established(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
		retval=on_cm_disconnected(event,rdma_trans);
		break;
	case RDMA_CM_EVENT_CONNECT_ERROR:
		rdma_trans->ctx->is_stop=1;
	default:
		/*occur an error*/
		retval=-1;
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
		
		/*
		 * note: rdma ack cm event will clear event content
		 * so need to copy event content into event_copy.
		 */
		rdma_ack_cm_event(event);

		if(hmr_handle_ec_event(&event_copy))
			break;
	}
	
	if(retval&&errno!=EAGAIN)
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
	return retval;
	
cleanec:
	rdma_destroy_event_channel(rdma_trans->event_channel);
	return retval;
}

static struct hmr_rdma_transport *hmr_rdma_transport_create(struct hmr_context *ctx)
{
	struct hmr_rdma_transport *rdma_trans;

	rdma_trans=(struct hmr_rdma_transport*)calloc(1,sizeof(struct hmr_rdma_transport));
	if(!rdma_trans){
		ERROR_LOG("allocate hmr_rdma_transport memory error.");
		return NULL;
	}
	rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_INIT;
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

	/*rdma_cm_id dont init the rdma_cm_id's verbs*/
	retval=rdma_create_id(rdma_trans->event_channel,
					&rdma_trans->cm_id,rdma_trans,RDMA_PS_TCP);
	if(retval){
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
	rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_CONNECTING;
	rdma_trans->is_client=1;
	return retval;
	
cleancmid:
	rdma_destroy_id(rdma_trans->cm_id);

cleanrdmatrans:
	rdma_trans->cm_id=NULL;
	
	return retval;
}

static int hmr_rdma_transport_listen(struct hmr_rdma_transport *rdma_trans)
{
	int retval=0,backlog,listen_port;
	struct sockaddr_in addr;
	
	
	retval=rdma_create_id(rdma_trans->event_channel,&rdma_trans->cm_id,
						rdma_trans,RDMA_PS_TCP);
	if(retval){
		ERROR_LOG("rdma create id error.");
		return retval;
	}
	
	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	
	retval=rdma_bind_addr(rdma_trans->cm_id,(struct sockaddr*)&addr);
	if(retval){
		ERROR_LOG("rdma bind addr error.");
		goto cleanid;
	}

	backlog=10;
	retval=rdma_listen(rdma_trans->cm_id,backlog);
	if(retval){
		ERROR_LOG("rdma listen error.");
		goto cleanid;
	}

	listen_port=ntohs(rdma_get_src_port(rdma_trans->cm_id));
	rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_LISTEN;
	INFO_LOG("rdma listening on port %d",listen_port);

	rdma_trans->is_client=0;
	return retval;
	
cleanid:
	rdma_destroy_id(rdma_trans->cm_id);
	rdma_trans->cm_id=NULL;
	
	return retval;
}

static struct hmr_rdma_transport *hmr_rdma_transport_accept(struct hmr_rdma_transport *rdma_trans)
{
	int err=0;
	struct rdma_conn_param conn_param;
	struct hmr_rdma_transport *accept_rdma_trans;

	while(!rdma_trans->accept_rdma_trans);
	
	memset(&conn_param, 0, sizeof(conn_param));

	INFO_LOG("discover new client.");
	
	accept_rdma_trans=rdma_trans->accept_rdma_trans;
	rdma_trans->accept_rdma_trans=accept_rdma_trans->accept_rdma_trans;
	accept_rdma_trans->accept_rdma_trans=NULL;
	
	err=rdma_accept(accept_rdma_trans->cm_id, &conn_param);
	if(err){
		ERROR_LOG("rdma accept error.");
		return NULL;
	}
	accept_rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_CONNECTING;
	accept_rdma_trans->normal_mempool=hmr_mempool_create(accept_rdma_trans, 0);
#ifdef HMR_NVM_ENABLE
	accept_rdma_trans->nvm_buffer=hmr_mempool_create(accept_rdma_trans,0);
	accept_rdma_trans->nvm_mempool=hmr_mempool_create(accept_rdma_trans,1);
#endif
	hmr_post_recv(accept_rdma_trans);

	return accept_rdma_trans;
}


struct hmr_rdma_transport_operations rdma_trans_ops={
	.init		= hmr_rdma_transport_init,
	.release	= hmr_rdma_transport_release,
	.create		= hmr_rdma_transport_create,
	.connect	= hmr_rdma_transport_connect,
	.listen		= hmr_rdma_transport_listen,
	.accept		= hmr_rdma_transport_accept
};	
