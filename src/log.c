// log.c - tiny logger
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

void log_init(const char *path, int foreground) {
    if (foreground) {
        g_logfp = stderr;
    } else {
        g_logfp = fopen(path ? path : "/tmp/hostd.log", "a");
        if (!g_logfp) g_logfp = stderr;
    }
}

void log_close(void) {
    if (g_logfp && g_logfp != stderr) fclose(g_logfp);
    g_logfp = NULL;
}

void log_msg(const char *fmt, ...) {
    if (!g_logfp) g_logfp = stderr;
    time_t t = time(NULL);
    struct tm tm; localtime_r(&t, &tm);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(g_logfp, "[%s] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_logfp, fmt, ap);
    va_end(ap);
    fflush(g_logfp);
}
