#pragma once
#include <stdio.h>
#include <stdbool.h>

extern volatile sig_atomic_t g_running;   // set false to stop server
extern FILE *g_logfp;
extern int g_verbose;

void log_init(const char *path, int foreground);
void log_close(void);
void log_msg(const char *fmt, ...);

int server_run(const char *sock_path);
