// server.c - UNIX socket server loop
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#include "hostd.h"
#include "protocol.h"
#include "log.h"

static int create_unix_listener(const char *path) {
    int fd = -1;
    struct sockaddr_un addr;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        log_msg("socket error: %s\n", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);
    // unlink existing
    unlink(path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_msg("bind(%s) error: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, 16) < 0) {
        log_msg("listen error: %s\n", strerror(errno));
        close(fd);
        unlink(path);
        return -1;
    }
    // restrict permissions
    chmod(path, 0600);
    return fd;
}

static void handle_client(int cfd) {
    char inbuf[4096];
    char outbuf[4096];
    ssize_t n;

    // Simple line-by-line processing
    while ((n = read(cfd, inbuf, sizeof(inbuf)-1)) > 0) {
        inbuf[n] = 0;

        // process each line separately
        char *saveptr = NULL;
        char *line = strtok_r(inbuf, "\r\n", &saveptr);
        while (line) {
            if (*line == 0) { line = strtok_r(NULL, "\r\n", &saveptr); continue; }

            int wr = protocol_handle_line(line, outbuf, sizeof(outbuf));
            if (wr < 0) {
                const char *err = "400 ERR internal\n";
                write(cfd, err, strlen(err));
            } else {
                write(cfd, outbuf, wr);
            }
            line = strtok_r(NULL, "\r\n", &saveptr);
        }
    }
}

int server_run(const char *sock_path) {
    int lfd = create_unix_listener(sock_path);
    if (lfd < 0) return 1;

    log_msg("listening on %s\n", sock_path);

    while (g_running) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            log_msg("accept error: %s\n", strerror(errno));
            break;
        }
        if (g_verbose) fprintf(stderr, "[hostd] client connected\n");
        handle_client(cfd);
        close(cfd);
        if (g_verbose) fprintf(stderr, "[hostd] client disconnected\n");
    }

    close(lfd);
    unlink(sock_path);
    return 0;
}
