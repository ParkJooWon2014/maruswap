LIBDMS = libib.a

CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

OFLAGS =

# Features
CFLAGS += $(OFLAGS)
CFLAGS += -I../include
CFLAGS += -D_POSIX_C_SOURCE -D_GNU_SOURCE -D_ISOC99_SOURCE
CFLAGS += -c -g -fPIC
CFLAGS += -std=gnu99 -Wall -Werror


TARGETS = $(LIBDMS)

all: $(TARGETS)

$(LIBDMS): ib.o rpc.o memblock.o handler.o test.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
.PHONY: clean
clean:
	rm -f $(TARGETS) *.o cscope.out

.PHONY: cscope
cscope:
	cscope -b -R
