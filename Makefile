.PHONY:clean

CC :=gcc
CFLAGS := -Wall -g
LDFLAGS	:= ${LDFLAGS} -lrdmacm -libverbs -lpthread

all:test
	
hmr_rdma_transport.o:hmr_rdma_transport.c
	gcc -c $^ -o $@ ${LDFLAGS}
	
test:hmr_log.o hmr_context.o hmr_rdma_transport.o test.o
	gcc	$^ -o $@ ${LDFLAGS}
	
clean:
	rm -rf *.o
	rm -rf test
