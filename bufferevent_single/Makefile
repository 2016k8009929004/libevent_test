TARGET = server client
all: $(TARGET)

CC = g++
LD = g++

CFLAGS = -std=c++11 -g -Wall -Iinclude -I./Hikv/ntstore -I./Hikv/mem -I./Hikv/lib -I./Hikv/obj -I./Hikv/tbb -I./Hikv/pmdk/include 
LDFLAGS = -L/usr/local/lib/ -L ./third-party/jemalloc-4.2.1/lib -L ./third-party/tbb 

LIBS = -lpthread -levent -ljemalloc -ltbb -lpmem

HDRS = ./include/*.h 

SRCS = client.cc server.cc

OBJS = $(patsubst %.cc,%.o, $(SRCS))

$(OBJS)  : %.o : %.cc 
		$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
		$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS)
kv:
		$(CC) -std=c++11 $(SRCS) ./Hikv/obj/threadpool.cc ./Hikv/obj/btree.cc ./Hikv/mem/pm_alloc.cc ./Hikv/lib/city.cc ./Hikv/lib/pflush.c ./Hikv/ntstore/ntstore.c -Iinclude -I./Hikv/ntstore -I./Hikv/mem -I./Hikv/lib -I./Hikv/obj -I./Hikv/tbb -I./Hikv/pmdk/include -o tset -L ./third-party/jemalloc-4.2.1/lib -ljemalloc -L ./third-party/tbb -ltbb -lpmem -lpthread -levent

server.o: server.cc
		$(CC) -c $(CFLAGS) $< -o $@

server: server.o
		$(LD) $< $(LDFLAGS) -o $@ $(LIBS)

client.o: client.cc
		$(CC) -c $(CFLAGS) $< -o $@

client: client.o
		$(LD) $< $(LDFLAGS) -o $@ $(LIBS)

clean:
		rm -f *.o $(TARGET)

