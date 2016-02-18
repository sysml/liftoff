RM = rm -f
CC = gcc
LD = $(CC)
CFLAGS+=-Wall -Wextra -O3 -fdata-sections -ffunction-sections
LDFLAGS+=-Wl,--gc-sections
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
