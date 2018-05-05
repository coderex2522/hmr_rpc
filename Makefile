.PHONY:clean

CC :=gcc
CFLAGS := -Wall -g
LDFLAGS	:= ${LDFLAGS} -lrdmacm -libverbs -lpthread

all:client server

hmr_mem.o:hmr_mem.c
	gcc -c $^ -o $@ ${LDFLAGS}
	
hmr_rdma_transport.o:hmr_rdma_transport.c
	gcc -c $^ -o $@ ${LDFLAGS}
	
client:hmr_log.o hmr_context.o hmr_mem.o hmr_rdma_transport.o client.o
	gcc	$^ -o $@ ${LDFLAGS}

server:hmr_log.o hmr_context.o hmr_mem.o hmr_rdma_transport.o server.o
	gcc $^ -o $@ ${LDFLAGS}
	
clean:
	rm -rf *.o
	rm -rf client
	rm -rf server