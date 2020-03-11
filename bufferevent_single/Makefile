TARGET = libevent_test
all: $(TARGET)

CC = g++
LD = g++

CFLAGS = -std=c++11 -g -Wall -Iinclude -I./HiKV/ntstore -I./HiKV/mem -I./HiKV/lib -I./HiKV/obj -I./HiKV/tbb -I./HiKV/pmdk/include 
LDFLAGS = -L/usr/local/lib/ -L ./third-party/jemalloc-4.2.1/lib -L ./third-party/tbb 

LIBS = -lpthread -levent -ljemalloc -ltbb -lpmem -lpthread

HDRS = ./include/*.h 

SRCS = common.cc evallib.cc client.cc server.cc main.cc

OBJS = $(patsubst %.cc,%.o, $(SRCS))

$(OBJS)  : %.o : %.cc 
		$(CC) -c $(CFLAGS) $(HDRS) $< -o $@

$(TARGET): $(OBJS)
		$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

clean:
		rm -f *.o $(TARGET)

