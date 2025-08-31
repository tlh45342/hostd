#pragma once
#include <stdio.h>

void log_init(const char *path, int foreground);
void log_close(void);
void log_msg(const char *fmt, ...);

extern FILE *g_logfp;
