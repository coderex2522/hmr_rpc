#include <sys/eventfd.h>

#include "hmr_base.h"
#include "hmr_log.h"
#include "hmr_epoll.h"
#include "hmr_context.h"


struct hmr_context *hmr_context_create()
{
	struct hmr_context *ctx;

	ctx=(struct hmr_context *)calloc(1,sizeof(struct hmr_context));
	if(!ctx){
		ERROR_LOG("allocate hmr_context memory error.");
		return NULL;
	}
	
	ctx->epfd=epoll_create(HMR_EPOLL_INIT_SIZE);
	if(ctx->epfd<0){
		ERROR_LOG("create epoll fd error.");
		goto cleanctx;
	}
	INFO_LOG("hmr context create success.");
	return ctx;
	
cleanctx:
	free(ctx);
	return NULL;
}


int hmr_context_add_event_fd(struct hmr_context *ctx,int fd,int events,hmr_event_handler event_handler,void *data)
{
	struct epoll_event ee;
	struct hmr_event_data *event_data;
	int retval=0;
	
	event_data=(struct hmr_event_data*)calloc(1,sizeof(struct hmr_event_data));
	if(!event_data){
		ERROR_LOG("%s allocate memory error.",__func__);
		return -1;
	}

	event_data->fd=fd;
	event_data->data=data;
	event_data->event_handler=event_handler;
	
	memset(&ee,0,sizeof(ee));
	ee.events=events;
	ee.data.ptr=event_data;

	retval=epoll_ctl(ctx->epfd,EPOLL_CTL_ADD,fd,&ee);
	if(retval){
		ERROR_LOG("Context add event fd error.");
		free(event_data);
	}
	else
		INFO_LOG("hmr context add event fd success.");
	return retval;
}

int hmr_context_listen_fd(struct hmr_context *ctx)
{
	struct epoll_event events[1024];
	struct hmr_event_data *event_data;
	int i,k,events_nr=0;

	for(k=0;k<20;k++){
		events_nr=epoll_wait(ctx->epfd,events,ARRAY_SIZE(events),5000);
		if(events_nr>0){
			INFO_LOG("events_nr %d",events_nr);
			for(i=0;i<events_nr;i++){
				INFO_LOG("process events[%d]",i);
				event_data=(struct hmr_event_data*)events[i].data.ptr;
				event_data->event_handler(event_data->fd,event_data->data);
			}
		}
	}

	return 0;
}
