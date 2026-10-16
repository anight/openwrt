CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -Wno-unused-parameter -std=gnu11

PROTOS = proto-mdns.o proto-ssdp.o

all: mcast-router mcr-probe

mcast-router: mcast-router.o $(PROTOS)
	$(CC) $(LDFLAGS) -o $@ $^

mcr-probe: mcr-probe.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c mcr.h
	$(CC) $(CFLAGS) -c -o $@ $<

test-proto: test-proto.o $(PROTOS)
	$(CC) $(LDFLAGS) -o $@ $^

test: test-proto
	./test-proto

clean:
	rm -f *.o mcast-router mcr-probe test-proto

.PHONY: all test clean
