#ifndef HMR_BASE_H
#define HMR_BASE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>

#define HMR_EPOLL_INIT_SIZE 1024

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

#endif
