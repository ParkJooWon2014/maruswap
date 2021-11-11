CC		= gcc

INCLUDE = -I../include
LIBIB= libib.a

CFLAGS += $(INCLUDE)
CFLAGS += -c -g
CFLAGS += -D_POSIX_C_SOURCE -D_GNU_SOURCE
CFLAGS += -std=gnu99 -Wall -Werror

LDFLAGS += -L../lib
LDFLAGS += -lpthread -libverbs -lrdmacm -l:$(LIBIB)

TARGETS = server

all: $(TARGETS)

server: main.o 
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGETS) cscope.out tags

.PHONY: cscope
cscope:
	cscope -b -R
