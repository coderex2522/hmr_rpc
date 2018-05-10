.PHONY:clean

CC :=gcc
CFLAGS := -Wall -g
LDFLAGS	:= ${LDFLAGS} -lrdmacm -libverbs -lpthread

all:client server

hmr_mem.o:hmr_mem.c
	gcc -Wall -g	-c -o $@ $^ ${LDFLAGS}
	
hmr_rdma_transport.o:hmr_rdma_transport.c
	gcc -Wall -g	-c -o $@ $^ ${LDFLAGS}
	
client:hmr_log.o hmr_context.o hmr_mem.o hmr_rdma_transport.o hmr_task.o hmr_utils.o hmr_timerfd.o client.o
	gcc -Wall -g $^ -o $@ ${LDFLAGS}

server:hmr_log.o hmr_context.o hmr_mem.o hmr_rdma_transport.o hmr_task.o hmr_utils.o hmr_timerfd.o server.o
	gcc -Wall -g $^ -o $@ ${LDFLAGS}
	
clean:
	rm -rf *.o
	rm -rf client
	rm -rf server