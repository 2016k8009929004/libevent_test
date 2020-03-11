TARGET = libevent_test
all: $(TARGET)

CC = g++
LD = g++

CFLAGS = -std=c++11 -g -Wall -Iinclude -I./Hikv/ntstore -I./Hikv/mem -I./Hikv/lib -I./Hikv/obj -I./Hikv/tbb -I./Hikv/pmdk/include 
LDFLAGS = -L/usr/local/lib/ -L ./third-party/jemalloc-4.2.1/lib -L ./third-party/tbb 

LIBS = -lpthread -levent -ljemalloc -ltbb -lpmem

HDRS = ./include/*.h 

SRCS = common.cc evallib.cc client.cc server.cc main.cc

OBJS = $(patsubst %.cc,%.o, $(SRCS))

$(OBJS)  : %.o : %.cc 
		$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
		$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS)
kv:
		$(CC) -std=c++11 $(SRCS) ./Hikv/obj/threadpool.cc ./Hikv/obj/btree.cc ./Hikv/mem/pm_alloc.cc ./Hikv/lib/city.cc ./Hikv/lib/pflush.c ./Hikv/ntstore/ntstore.c -I./Hikv/ntstore -I./Hikv/mem -I./Hikv/lib -I./Hikv/obj -I./Hikv/tbb -I./Hikv/pmdk/include -o tset -L ./third-party/jemalloc-4.2.1/lib -ljemalloc -L ./third-party/tbb -ltbb -lpmem -lpthread


clean:
		rm -f *.o $(TARGET)

