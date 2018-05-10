#ifndef HMR_TIMERFD_H
#define HMR_TIMERFD_H

int hmr_timerfd_create(struct itimerspec *new_value);
void hmr_send_task_handler(int fd,void *data);
void hmr_sync_nvm_handler(int fd,void *data);
#endif