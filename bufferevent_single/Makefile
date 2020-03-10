TARGET = libevent_test
all: $(TARGET)

CC = g++
LD = g++

CFLAGS = -g -Wall -Iinclude
LDFLAGS = -L/usr/local/lib/ 

LIBS = -lpthread -levent

HDRS = ./include/*.h

SRCS = common.cc evallib.cc client.cc server.cc main.cc

OBJS = $(patsubst %.cc,%.o, $(SRCS))

$(OBJS)  : %.o : %.cc include/*.h
		$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
		$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

clean:
		rm -f *.o $(TARGET)

