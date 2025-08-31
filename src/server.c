// server.c - UNIX and TCP server loop
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "hostd.h"
#include "protocol.h"
#include "log.h"

static int create_unix_listener(const char *path) {
    int fd = -1;
    struct sockaddr_un addr;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        log_msg("socket(AF_UNIX) error: %s\n", strerror(errno));
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

static int create_tcp_listener(const char *bind_host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { log_msg("socket(AF_INET) error: %s\n", strerror(errno)); return -1; }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (!bind_host || bind_host[0] == 0) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, bind_host, &addr.sin_addr) != 1) {
            // try DNS
            struct hostent *he = gethostbyname(bind_host);
            if (!he || he->h_addrtype != AF_INET) {
                log_msg("invalid bind host: %s\n", bind_host ? bind_host : "(null)");
                close(fd);
                return -1;
            }
            memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));
        }
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_msg("bind(tcp) error: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    if (listen(fd, 64) < 0) {
        log_msg("listen(tcp) error: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static void handle_client(int cfd) {
    char inbuf[4096];
    char outbuf[4096];
    ssize_t n;

    while ((n = read(cfd, inbuf, sizeof(inbuf)-1)) > 0) {
        inbuf[n] = 0;

        // process each line
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

    log_msg("listening (unix) on %s\n", sock_path);

    while (g_running) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            log_msg("accept error (unix): %s\n", strerror(errno));
            break;
        }
        if (g_verbose) fprintf(stderr, "[hostd] client connected (unix)\n");
        handle_client(cfd);
        close(cfd);
        if (g_verbose) fprintf(stderr, "[hostd] client disconnected (unix)\n");
    }

    close(lfd);
    unlink(sock_path);
    return 0;
}

int server_run_tcp(const char *bind_host, int bind_port) {
    int lfd = create_tcp_listener(bind_host, bind_port);
    if (lfd < 0) return 1;

    log_msg("listening (tcp) on %s:%d\n", bind_host && bind_host[0]?bind_host:"0.0.0.0", bind_port);

    while (g_running) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            log_msg("accept error (tcp): %s\n", strerror(errno));
            break;
        }
        if (g_verbose) fprintf(stderr, "[hostd] client connected (tcp)\n");
        handle_client(cfd);
        close(cfd);
        if (g_verbose) fprintf(stderr, "[hostd] client disconnected (tcp)\n");
    }

    close(lfd);
    return 0;
}
