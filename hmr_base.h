#ifndef HMR_BASE_H
#define HMR_BASE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <linux/list.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#define HMR_EPOLL_INIT_SIZE 1024
#define HMR_MAX_DATA_ENTRY 4

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif
#define min(a,b) (a>b?b:a)

enum hmr_log_level{
	HMR_LOG_LEVEL_ERROR,
	HMR_LOG_LEVEL_WARN,
	HMR_LOG_LEVEL_INFO,
	HMR_LOG_LEVEL_DEBUG,
	HMR_LOG_LEVEL_TRACE,
	HMR_LOG_LEVEL_LAST
};

/**
 * check the rdma device is exist.
 */
int hmr_rdma_dev_is_exist();


/*the structure about hmr  msg*/
enum hmr_msg_type{
	HMR_MSG_COMMON,
	HMR_MSG_READ,
	HMR_MSG_WRITE
};

struct hmr_iovec{
	void *iov_base;
	size_t iov_len;
	struct hmr_iovec *next;
};

struct hmr_msg{
	enum hmr_msg_type msg_type;
	int nents;
	struct hmr_iovec *data;
};

#endif
