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
#include "hmr_task.h"

static int rdma_num_devices=0;

pthread_once_t init_pthread=PTHREAD_ONCE_INIT;
pthread_once_t release_pthread=PTHREAD_ONCE_INIT;

LIST_HEAD(dev_list);

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

static void hmr_rdma_init_handle(void)
{
	struct ibv_context **ctx_list;
	struct hmr_device *dev;
	int i,num_devices=0;
	
	ctx_list=rdma_get_devices(&num_devices);
	if(!ctx_list)
	{
		ERROR_LOG("Failed to get the rdma device list.");
		return ;
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
}

void hmr_rdma_init()
{
	pthread_once(&init_pthread,hmr_rdma_init_handle);
}
	
static void hmr_rdma_dev_release(struct hmr_device *dev)
{
	list_del(&dev->dev_list_entry);
	ibv_dealloc_pd(dev->pd);
	free(dev);
}

static void hmr_rdma_release_handle(void)
{
	struct hmr_device	*dev, *next;

	/* free devices */
	list_for_each_entry_safe(dev, next, &dev_list, dev_list_entry) {
		list_del_init(&dev->dev_list_entry);
		hmr_rdma_dev_release(dev);
	}
}

void hmr_rdma_release()
{
	pthread_once(&release_pthread, hmr_rdma_release_handle);
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

static void hmr_post_recv(struct hmr_rdma_transport *rdma_trans)
{
	struct ibv_recv_wr recv_wr,*bad_wr=NULL;
	struct hmr_task *recv_task;
	struct ibv_sge sge;
	int err=0;

	recv_task=hmr_recv_task_create(rdma_trans);
	if(!recv_task){
		ERROR_LOG("post recv error.");
		return ;
	}
	
	/*when call the ibv_poll_cq,we can get the context from wr_id*/
	recv_wr.wr_id=(uintptr_t)recv_task;
	recv_wr.next=NULL;
	recv_wr.sg_list=&sge;
	recv_wr.num_sge=1;

	sge.addr=(uintptr_t)recv_task->sge_list.addr;
	sge.length=recv_task->sge_list.length;
	sge.lkey=recv_task->sge_list.lkey;

	err=ibv_post_recv(rdma_trans->qp, &recv_wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv post recv error.");
	}
}

static void hmr_handle_close_connection(struct hmr_task *task, struct ibv_wc *wc)
{
	struct hmr_rdma_transport *rdma_trans=task->rdma_trans; 
	struct hmr_msg msg;
	
	if(wc->opcode==IBV_WC_RECV&&task->task_type==HMR_TASK_FINISH){
		msg.msg_type=HMR_MSG_DONE;
		msg.data=strdup("the connection is closing.");
		msg.data_size=strlen(msg.data)+1;
		hmr_rdma_send(rdma_trans, &msg);
	}
	else if(wc->opcode==IBV_WC_SEND&&task->task_type==HMR_TASK_DONE){
		INFO_LOG("call disconnect.");
		rdma_disconnect(rdma_trans->cm_id);
	}
	else if(wc->opcode==IBV_WC_RECV&&task->task_type==HMR_TASK_DONE){
		INFO_LOG("call disconnect.");
		rdma_disconnect(rdma_trans->cm_id);
	}
}

static void hmr_exchange_mr_info(struct hmr_rdma_transport *rdma_trans)
{
	struct hmr_msg msg;
	
	msg.msg_type=HMR_MSG_MR;
	msg.data=rdma_trans->normal_mempool->recv_mr;
	msg.data_size=sizeof(struct ibv_mr);
	
	INFO_LOG("%s recv addr %p",__func__,rdma_trans->normal_mempool->recv_mr->addr);
	hmr_rdma_send(rdma_trans, &msg);		
}


static void hmr_wc_success_handler(struct ibv_wc *wc)
{
	struct hmr_task *task;
	struct hmr_rdma_transport *rdma_trans;
	struct ibv_mr *mr;
	char *str;
	int i;
	
	task=(struct hmr_task*)(uintptr_t)wc->wr_id;
	rdma_trans=task->rdma_trans;
	
	switch (wc->opcode)
	{
	case IBV_WC_SEND:
		INFO_LOG("IBV_WC_SEND send the content [%s] success.",task->sge_list.addr+sizeof(enum hmr_msg_type));
		break;
	case IBV_WC_RECV:
		task->task_type=*(enum hmr_task_type*)task->sge_list.addr;
		INFO_LOG("IBV_WC_RECV recv the content [%s] success.",task->sge_list.addr+sizeof(enum hmr_msg_type));
		rdma_trans->cur_recv_num--;
		if(rdma_trans->cur_recv_num<MIN_RECV_NUM){
			for(i=0;i<INC_RECV_NUM;i++){
				hmr_post_recv(rdma_trans);
				rdma_trans->cur_recv_num++;
			}
		}
		str=(char*)(task->sge_list.addr+sizeof(enum hmr_msg_type));
		if(str[0]=='1'||str[0]=='2'){
			INFO_LOG("haha. %p",rdma_trans->normal_mempool->recv_mr->addr);
			INFO_LOG("content %s",rdma_trans->normal_mempool->recv_mr->addr+sizeof(enum hmr_msg_type));
		}
		break;
	case IBV_WC_RDMA_WRITE:
		break;
	case IBV_WC_RDMA_READ:
		break;
	default:
		ERROR_LOG("unknown opcode:%s",ibv_wc_opcode_str(wc->opcode));
		break;
	}
	if(task->task_type>=HMR_TASK_FINISH)
		hmr_handle_close_connection(task, wc);
	if(task->task_type==HMR_TASK_MR){
		if(wc->opcode==IBV_WC_SEND&&rdma_trans->trans_state==HMR_RDMA_TRANSPORT_STATE_CONNECTED)
			rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_SCONNECTED;
		
		if(wc->opcode==IBV_WC_RECV){
			memcpy(&rdma_trans->peer_info.normal_mr,task->sge_list.addr+sizeof(enum hmr_msg_type),sizeof(struct ibv_mr));
			INFO_LOG("normal mr %u %u",rdma_trans->peer_info.normal_mr.lkey,rdma_trans->peer_info.normal_mr.rkey);
			
			if(rdma_trans->trans_state==HMR_RDMA_TRANSPORT_STATE_CONNECTED)
				hmr_exchange_mr_info(rdma_trans);
			rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_RCONNECTED;
		}
	}
}

static void hmr_wc_error_handler(struct ibv_wc *wc)
{
	if(wc->status==IBV_WC_WR_FLUSH_ERR)
		INFO_LOG("work request flush error.");
	else
		ERROR_LOG("wc status [%s] is error.",ibv_wc_status_str(wc->status));

}

static void hmr_cq_comp_channel_handler(int fd, void *data)
{
	struct hmr_cq *hcq=(struct hmr_cq*)data;
	struct ibv_cq *cq;
	void *cq_context;
	struct ibv_wc wc;
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
		if(wc.status==IBV_WC_SUCCESS)
			hmr_wc_success_handler(&wc);
		else
			hmr_wc_error_handler(&wc);

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
	qp_init_attr.cap.max_send_sge=min(rdma_trans->device->device_attr.max_sge,MAX_SEND_SGE);
	
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


/**
 * @param[in]
 * 
 * @return 0 on success, other on error.
 */
static int on_cm_route_resolved(struct rdma_cm_event *event, struct hmr_rdma_transport *rdma_trans)
{
	struct rdma_conn_param conn_param;
	int i,retval=0;
	
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

	for(i=0;i<MIN_RECV_NUM*2;i++){
		hmr_post_recv(rdma_trans);
		rdma_trans->cur_recv_num++;
	}
	return retval;
	
cleanqp:
	hmr_qp_release(rdma_trans);
	rdma_trans->ctx->is_stop=1;
	rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_ERROR;
	return retval;
}

static int on_cm_connect_request(struct rdma_cm_event *event, struct hmr_rdma_transport *rdma_trans)
{
	struct rdma_conn_param conn_param;
	struct hmr_rdma_transport *accept_rdma_trans;
	int retval=0;

	INFO_LOG("event id %p rdma_trans cm_id %p event_listenid %p",event->id,rdma_trans->cm_id,event->listen_id);
	accept_rdma_trans=hmr_rdma_create(rdma_trans->ctx);
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

	if(rdma_trans->is_client)
		hmr_exchange_mr_info(rdma_trans);
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
		rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_ERROR;
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

static int hmr_rdma_event_channel_init(struct hmr_rdma_transport *rdma_trans)
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

struct hmr_rdma_transport *hmr_rdma_create(struct hmr_context *ctx)
{
	struct hmr_rdma_transport *rdma_trans;

	rdma_trans=(struct hmr_rdma_transport*)calloc(1,sizeof(struct hmr_rdma_transport));
	if(!rdma_trans){
		ERROR_LOG("allocate hmr_rdma_transport memory error.");
		return NULL;
	}
	rdma_trans->trans_state=HMR_RDMA_TRANSPORT_STATE_INIT;
	rdma_trans->ctx=ctx;
	hmr_rdma_event_channel_init(rdma_trans);
	INIT_LIST_HEAD(&rdma_trans->send_task_list);
	return rdma_trans;
}

static int hmr_port_uri_init(struct hmr_rdma_transport *rdma_trans,
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

int hmr_rdma_connect(struct hmr_rdma_transport* rdma_trans,
								const char *url, const char*port)
{
	int retval=0;
	if(!url||!port){
		ERROR_LOG("Url or port input error.");
		return -1;
	}

	retval=hmr_port_uri_init(rdma_trans, url, port);
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

int hmr_rdma_listen(struct hmr_rdma_transport *rdma_trans)
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

struct hmr_rdma_transport *hmr_rdma_accept(struct hmr_rdma_transport *rdma_trans)
{
	int i,err=0;
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

	/*pre commit some post recv*/
	for(i=0;i<MIN_RECV_NUM*2;i++){
		hmr_post_recv(accept_rdma_trans);
		accept_rdma_trans->cur_recv_num++;
	}
	return accept_rdma_trans;
}

static int hmr_msg_check(struct hmr_rdma_transport *rdma_trans, struct hmr_msg *msg)
{
	int retval=0;

	if(msg->msg_type!=HMR_MSG_READ&&msg->msg_type!=HMR_MSG_WRITE){
		while(rdma_trans->trans_state!=HMR_RDMA_TRANSPORT_STATE_CONNECTED&&
			rdma_trans->trans_state!=HMR_RDMA_TRANSPORT_STATE_SCONNECTED&&
			rdma_trans->trans_state!=HMR_RDMA_TRANSPORT_STATE_RCONNECTED){
			
			if(rdma_trans->trans_state>HMR_RDMA_TRANSPORT_STATE_RCONNECTED)
				return -1;
		}
	}
	else{
		while(rdma_trans->trans_state!=HMR_RDMA_TRANSPORT_STATE_RCONNECTED){
			if(rdma_trans->trans_state>HMR_RDMA_TRANSPORT_STATE_RCONNECTED)
			return -1;
		}
	}

	return retval;
}

static enum ibv_wr_opcode hmr_get_msg_opcode(struct hmr_msg *msg)
{
	enum ibv_wr_opcode retval=IBV_WR_SEND;
	
	switch (msg->msg_type)
	{
	case HMR_MSG_READ:
		retval=IBV_WR_RDMA_READ;
		break;
	case HMR_MSG_WRITE:
		retval=IBV_WR_RDMA_WRITE;
		break;
	}
	
	return retval;
}

int hmr_rdma_send(struct hmr_rdma_transport *rdma_trans, struct hmr_msg *msg)
{
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct hmr_task *send_task;
	int err=0;

	err=hmr_msg_check(rdma_trans, msg);
	if(err){
		ERROR_LOG("rdma transport exit error.");
		return -1;
	}

	send_task=hmr_send_task_create(rdma_trans,msg);
	send_task->task_type=msg->msg_type;

	memset(&send_wr,0,sizeof(send_wr));

	send_wr.wr_id=(uintptr_t)send_task;
	send_wr.num_sge=1;
	send_wr.opcode=hmr_get_msg_opcode(msg);
	send_wr.sg_list=&sge;
	send_wr.send_flags=IBV_SEND_SIGNALED;

	if(send_wr.opcode==IBV_WR_RDMA_READ||send_wr.opcode==IBV_WR_RDMA_WRITE){
		INFO_LOG("%s post rdma write. %p",__func__,rdma_trans->peer_info.normal_mr.addr);
		send_wr.wr.rdma.remote_addr=(uintptr_t)(rdma_trans->peer_info.normal_mr.addr);
		send_wr.wr.rdma.rkey=rdma_trans->peer_info.normal_mr.rkey;
	}
	
	sge.addr=(uintptr_t)send_task->sge_list.addr;
	sge.length=send_task->sge_list.length;
	sge.lkey=send_task->sge_list.lkey;
	
	err=ibv_post_send(rdma_trans->qp, &send_wr, &bad_wr);
	if(err){
		ERROR_LOG("ibv post send error.");
	}
	
	return 0;
}

