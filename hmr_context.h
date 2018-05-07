#ifndef HMR_CONTEXT_H
#define HMR_CONTEXT_H

struct hmr_context{
	int epfd;
	int is_stop;
	pthread_t epoll_pthread;
};

struct hmr_context *hmr_context_create();

int hmr_context_add_event_fd(struct hmr_context *ctx,int fd,int events,
						hmr_event_handler event_handler,void *data);

int hmr_context_del_event_fd(struct hmr_context *ctx,int fd);

void *hmr_context_run(void *data);

#endif