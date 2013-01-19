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
	./sample 2>&1 | grep "< HTTP"

sample: sample.o
	$(CC) -L. -lrcvr -lcurl $< -o $@

sample.o: sample.c librcvr.a

clean:
	$(RM) $(OBJS) sample.o sample librcvr.a
