// hostd.c - entrypoint
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "hostd.h"
#include "log.h"
#include "daemonize.h"
#include "libvm.h"

volatile sig_atomic_t g_running = 1;
FILE *g_logfp = NULL;
int g_verbose = 0;

static const char *DEFAULT_SOCK = "/tmp/hostd.sock";
static const char *DEFAULT_LOG  = "/tmp/hostd.log";
static const char *DEFAULT_PID  = NULL;

static void on_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "hostd (strawman) " HOSTD_VERSION "\n"
        "Usage: %s [-f] [-S socket] [-l logfile] [-p pidfile] [-v] [-V]\n"
        "  -f            Run in foreground (do not daemonize)\n"
        "  -S <socket>   UNIX socket path (default: %s)\n"
        "  -l <logfile>  Log file path (default: %s)\n"
        "  -p <pidfile>  Write PID to this file when daemonized\n"
        "  -v            Verbose logging to stderr (foreground only)\n"
        "  -V            Show version and exit\n",
        prog, DEFAULT_SOCK, DEFAULT_LOG);
}

int main(int argc, char **argv) {
    const char *sock_path = DEFAULT_SOCK;
    const char *log_path  = DEFAULT_LOG;
    const char *pid_path  = DEFAULT_PID;
    int foreground = 0;

    int opt;
    while ((opt = getopt(argc, argv, "fS:l:p:vVh")) != -1) {
        switch (opt) {
            case 'f': foreground = 1; break;
            case 'S': sock_path = optarg; break;
            case 'l': log_path = optarg; break;
            case 'p': pid_path = optarg; break;
            case 'v': g_verbose++; break;
            case 'V': printf("hostd " HOSTD_VERSION "\n"); return 0;
            case 'h': default: usage(argv[0]); return opt=='h'?0:1;
        }
    }

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (!foreground) {
        if (daemonize(pid_path, log_path, false) != 0) {
            fprintf(stderr, "daemonize failed: %s\n", strerror(errno));
            return 1;
        }
    } else {
        log_init(log_path, 1);
    }

    log_msg("hostd " HOSTD_VERSION " starting (socket=%s)\n", sock_path);

    if (vm_init() != 0) {
        log_msg("vm_init failed\n");
        return 1;
    }

    int rc = server_run(sock_path);

    vm_shutdown();
    log_msg("hostd exiting (rc=%d)\n", rc);
    log_close();
    return rc;
}
