DESTDIR ?= /usr/sbin
TARGET = xsiostat
OBJS = xsiostat.o xsiostat_vbd.o xsiostat_flt.o

CC = gcc
CFLAGS = -Wall -O3
LDFLAGS = -lxenstore

.PHONY: build
build: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: install
install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)/$(TARGET)
