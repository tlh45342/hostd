#pragma once
#include <stdbool.h>

int daemonize(const char *pidfile, const char *logfile, bool keep_stdout);
int write_pidfile(const char *pidfile);
