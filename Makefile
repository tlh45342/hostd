# hostd strawman - Makefile
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -g -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE
LDFLAGS ?=
INC     ?= -Iinclude

PREFIX       ?= /usr/local
BINDIR       ?= $(PREFIX)/bin
UNITDIR      ?= /etc/systemd/system
SERVICE_NAME ?= hostd

SRC = src/hostd.c src/server.c src/protocol.c src/libvm_stub.c src/log.c src/daemonize.c

.PHONY: all clean install uninstall

all: hostd vim-cmd

hostd: $(SRC)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC) $(LDFLAGS)

vim-cmd: examples/vim-cmd.c
	$(CC) $(CFLAGS) $(INC) -o $@ $< $(LDFLAGS)

clean:
	rm -f hostd vim-cmd
	rm -f *.o src/*.o

install: hostd vim-cmd
	# Binaries
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 hostd   $(DESTDIR)$(BINDIR)/hostd
	install -m 0755 vim-cmd $(DESTDIR)$(BINDIR)/vim-cmd
	# Optional: systemd unit (only if present in repo at systemd/$(SERVICE_NAME).service)
	@if [ -f systemd/$(SERVICE_NAME).service ]; then \
	  install -d $(DESTDIR)$(UNITDIR); \
	  install -m 0644 systemd/$(SERVICE_NAME).service $(DESTDIR)$(UNITDIR)/$(SERVICE_NAME).service; \
	  if command -v systemctl >/dev/null 2>&1; then systemctl daemon-reload; fi; \
	  echo "Installed systemd unit: $(UNITDIR)/$(SERVICE_NAME).service"; \
	else \
	  echo "Note: systemd/$(SERVICE_NAME).service not found; skipping unit install."; \
	fi
	@echo "Install complete."

uninstall:
	# Remove systemd unit (if present) and reload units (does NOT stop/disable)
	- rm -f $(DESTDIR)$(UNITDIR)/$(SERVICE_NAME).service
	- if command -v systemctl >/dev/null 2>&1; then systemctl daemon-reload; fi
	# Remove binaries
	- rm -f $(DESTDIR)$(BINDIR)/hostd
	- rm -f $(DESTDIR)$(BINDIR)/vim-cmd
	@echo "Uninstall complete. (If the service was running, it may still be active.)"
