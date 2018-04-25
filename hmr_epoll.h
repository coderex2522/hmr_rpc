#ifndef HMR_EPOLL_H
#define HMR_EPOLL_H

typedef void (*hmr_event_handler)(int fd,void *data);

struct hmr_event_data{
	int fd;
	void *data;
	hmr_event_handler event_handler;
};

#endif