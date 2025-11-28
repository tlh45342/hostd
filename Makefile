# Makefile for hostd (no vim-cmd)

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
LDFLAGS ?=
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/sbin
SYSTEMD_DIR ?= /etc/systemd/system

TARGET  = hostd

SRCS    = hostd.c \
          server.c \
          protocol.c \
          log.c \
          daemonize.c \
          libvm_stub.c

OBJS    = $(SRCS:.c=.o)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

install: $(TARGET)
	@echo "Installing $(TARGET) to $(DESTDIR)$(BINDIR)"
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/

	@# Optional: install systemd service if present
	if [ -f hostd.service ]; then \
		echo "Installing hostd.service to $(DESTDIR)$(SYSTEMD_DIR)"; \
		mkdir -p $(DESTDIR)$(SYSTEMD_DIR); \
		install -m 0644 hostd.service $(DESTDIR)$(SYSTEMD_DIR)/; \
	else \
		echo "hostd.service not found (skipping systemd unit install)"; \
	fi

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	if [ -f $(DESTDIR)$(SYSTEMD_DIR)/hostd.service ]; then \
		rm -f $(DESTDIR)$(SYSTEMD_DIR)/hostd.service; \
	fi
