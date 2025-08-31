// examples/vim-cmd.c - tiny client for hostd (UNIX or TCP)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

static const char *DEFAULT_SOCK = "/tmp/hostd.sock";

static int connect_unix(const char *sock) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket(AF_UNIX)"); return -1; }
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect(unix)");
        close(fd); return -1;
    }
    return fd;
}

static int connect_tcp(const char *hostport) {
    char host[128]={0};
    char port[16]={0};
    const char *colon = strchr(hostport, ':');
    if (!colon) { fprintf(stderr, "-T expects host:port\n"); return -1; }
    size_t hl = (size_t)(colon - hostport);
    if (hl >= sizeof(host)) { fprintf(stderr, "host too long\n"); return -1; }
    memcpy(host, hostport, hl); host[hl]=0;
    snprintf(port, sizeof(port), "%s", colon+1);

    struct addrinfo hints, *res=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, port, &hints, &res);
    if (err) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); return -1; }

    int fd = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) perror("connect(tcp)");
    return fd;
}

int main(int argc, char **argv) {
    const char *sock = DEFAULT_SOCK;
    const char *tcp  = NULL;

    int argi = 1;
    while (argi < argc) {
        if (strcmp(argv[argi], "-S")==0 && argi+1 < argc) { sock = argv[argi+1]; argi+=2; continue; }
        if (strcmp(argv[argi], "-T")==0 && argi+1 < argc) { tcp = argv[argi+1]; argi+=2; continue; }
        break;
    }
    if (argi >= argc) {
        fprintf(stderr, "Usage: %s [-S socket] [-T host:port] COMMAND [ARGS...]\n", argv[0]);
        return 1;
    }

    size_t total = 0;
    for (int i=argi;i<argc;i++) total += strlen(argv[i]) + 1;
    char *line = malloc(total+2);
    if (!line) return 1;
    line[0] = 0;
    for (int i=argi;i<argc;i++) { strcat(line, argv[i]); if (i+1<argc) strcat(line, " "); }
    strcat(line, "\n");

    int fd = tcp ? connect_tcp(tcp) : connect_unix(sock);
    if (fd < 0) { free(line); return 1; }

    if (write(fd, line, strlen(line)) < 0) { perror("write"); free(line); close(fd); return 1; }
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    if (n < 0) { perror("read"); free(line); close(fd); return 1; }
    buf[n] = 0;
    fputs(buf, stdout);
    close(fd);
    free(line);
    return 0;
}
