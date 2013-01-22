AR= ar cq
RANLIB= ranlib

INCLUDES= -Isrc -Iinclude
CFLAGS= -g -Wall -Werror -std=c99 $(INCLUDES)
SRCS= $(wildcard src/*.c)
OBJS= $(patsubst %.c,%.o,$(SRCS))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: librcvr.a

librcvr.a: $(OBJS)
	$(RM) $@
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

demo: sample
	./sample

sample: sample.o tools/threadpool.o
	$(CC) -L. -lrcvr -lcurl $^ -o $@

sample.o: sample.c librcvr.a

clean:
	$(RM) $(OBJS) sample.o tools/threadpool.o sample librcvr.a
