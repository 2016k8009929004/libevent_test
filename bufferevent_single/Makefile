TARGET = server client
all: $(TARGET)

CC = g++
LD = g++

SERVER_CFLAGS = -Iinclude -I./Hikv/ntstore -I./Hikv/mem -I./Hikv/lib -I./Hikv/obj -I./Hikv/tbb -I./Hikv/pmdk/include 
CLIENT_CFLAGS = -Iinclude 
SERVER_LDFLAGS = -L/usr/local/lib/ -L ./third-party/jemalloc-4.2.1/lib -L ./third-party/tbb 
CLIENT_LDFLAGS = -L/usr/local/lib/ -L ./third-party/jemalloc-4.2.1/lib -L ./third-party/tbb 

SERVER_LIBS = -lpthread -levent -ljemalloc -ltbb -lpmem
CLIENT_LIBS = -lpthread -ljemalloc -ltbb -lpmem

HDRS = ./include/*.h 

SRCS = client.cc server.cc

KV_SRCS = ./Hikv/obj/threadpool.cc ./Hikv/obj/btree.cc ./Hikv/mem/pm_alloc.cc ./Hikv/lib/city.cc ./Hikv/lib/pflush.c ./Hikv/ntstore/ntstore.c

OBJS = $(patsubst %.cc,%.o, $(SRCS))

server:
		$(CC) -std=c++11 $(KV_SRCS) server.cc $(SERVER_CFLAGS) -o $@ $(SERVER_LDFLAGS) $(SERVER_LIBS)

client:
		$(CC) -std=c++11 client.cc $(CLIENT_CFLAGS) -o $@ $(CLIENT_LDFLAGS) $(CLIENT_LIBS)

clean:
		rm -f *.o $(TARGET)

