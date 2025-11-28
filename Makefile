# hostd strawman - Makefile
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -g -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE
LDFLAGS ?=
INC     ?= -Iinclude

PREFIX       ?= /usr/local
BINDIR       ?= $(PREFIX)/bin
UNITDIR      ?= /etc/systemd/system
SERVICE_NAME ?= hostd

# Source files for hostd
SRC = src/hostd.c src/server.c src/protocol.c src/libvm_stub.c src/log.c src/daemonize.c

.PHONY: all clean install uninstall

# Build only hostd
all: hostd

# Single-step build: compile & link directly from .c files
hostd: $(SRC)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f hostd
	rm -f *.o src/*.o

install: hostd
	# Binaries
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 hostd $(DESTDIR)$(BINDIR)/hostd
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
	# Remove binary
	- rm -f $(DESTDIR)$(BINDIR)/hostd
	@echo "Uninstall complete. (If the service was running, it may still be active.)"
