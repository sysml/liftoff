CONFIG_COMPILE_LTO    ?= y

RM = rm -f
CC = gcc
LD = $(CC)
CFLAGS += -Wall -Wextra -O3 -fdata-sections -ffunction-sections
LDFLAGS += -Wl,--gc-sections
CFLAGS-$(CONFIG_COMPILE_LTO)  += -flto
LDFLAGS-$(CONFIG_COMPILE_LTO) += $(CFLAGS) $(CFLAGS-y)
LDLIBS +=

%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS-y) -c $< -o $@

%: %.o
	$(CC) $(LDFLAGS) $(LDFLAGS-y) $^ $(LDLIBS) -o $@

.PHONY: default
default: all

all: liftoff

liftoff: liftoff.o

clean: 
	$(RM) *.o *~ core liftoff
