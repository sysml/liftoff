RM = rm -f
CC = gcc
LD = $(CC)
CFLAGS += -Wall -O3
LDFLAGS +=
LDLIBS +=

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: default
default: all

all: liftoff

liftoff: liftoff.o

clean: 
	$(RM) *.o *~ core liftoff
