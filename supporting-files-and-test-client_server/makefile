# obtained under the X11 license -- see "license" for details

CFLAGS=-g -Wall -pedantic

all: server client

server: server/server.o tcp.o common.o common.h
	gcc ${CFLAGS} -o server/server server/server.o common.o tcp.o -lpthread

client: client/client.o tcp.o common.o common.h
	gcc ${CFLAGS} -o client/client client/client.o common.o tcp.o -lpthread

server.o: server/server.c common.h
	gcc ${CFLAGS} -c server.c

client.o: client/client.c common.h
	gcc ${CFLAGS} -c client.c

tcp.o: tcp.c common.h
	gcc ${CFLAGS} -c tcp.c

common.o: common.c common.h
	gcc ${CFLAGS} -c common.c

clean:
	/bin/rm *.o client/client server/server
