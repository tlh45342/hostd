// daemonize.c - classic UNIX double-fork + pidfile
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "daemonize.h"
#include "log.h"

static const char *g_pidfile = NULL;

int write_pidfile(const char *pidfile) {
    if (!pidfile) return 0;
    FILE *fp = fopen(pidfile, "w");
    if (!fp) return -1;
    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    return 0;
}

int daemonize(const char *pidfile, const char *logfile, bool keep_stdout) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0); // parent exits

    if (setsid() < 0) return -1;

    signal(SIGHUP, SIG_IGN); // ignore hup in daemon
    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    umask(0);
    chdir("/");

    // redirect stdio
    int fd0 = open("/dev/null", O_RDONLY);
    int fd1 = open(logfile ? logfile : "/tmp/hostd.log", O_WRONLY|O_CREAT|O_APPEND, 0644);
    int fd2 = keep_stdout ? dup(fd1) : open("/dev/null", O_WRONLY);

    if (fd0 >= 0) dup2(fd0, STDIN_FILENO);
    if (fd1 >= 0) dup2(fd1, STDOUT_FILENO);
    if (fd2 >= 0) dup2(fd2, STDERR_FILENO);

    if (fd0 > 2) close(fd0);
    if (fd1 > 2) close(fd1);
    if (fd2 > 2) close(fd2);

    log_init(logfile, 0);

    g_pidfile = pidfile;
    if (g_pidfile && write_pidfile(g_pidfile) != 0) {
        log_msg("write_pidfile(%s) failed: %s\n", g_pidfile, strerror(errno));
    }
    return 0;
}
