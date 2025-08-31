# hostd strawman - Makefile
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -g -DHOSTD_VERSION=\"0.1.0\" -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE
LDFLAGS ?=

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

SRC = src/hostd.c src/server.c src/protocol.c src/libvm_stub.c src/log.c src/daemonize.c
INC = -Iinclude

all: hostd vim-cmd

hostd: $(SRC)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC) $(LDFLAGS)

vim-cmd: examples/vim-cmd.c
	$(CC) $(CFLAGS) $(INC) -o $@ examples/vim-cmd.c $(LDFLAGS)

clean:
	rm -f hostd vim-cmd
	rm -f *.o src/*.o

install: hostd
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 hostd $(DESTDIR)$(BINDIR)/hostd

.PHONY: all clean 

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
UNITDIR ?= /etc/systemd/system

install: hostd
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 hostd $(DESTDIR)$(BINDIR)/hostd
	# install systemd unit file
	install -d $(DESTDIR)$(UNITDIR)
	install -m 0644 systemd/hostd.service $(DESTDIR)$(UNITDIR)/hostd.service
	# reload systemd so it sees the new service
	systemctl daemon-reload
	# optionally enable on install
	# systemctl enable hostd
