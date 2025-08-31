// examples/vim-cmd.c - tiny client for hostd
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static const char *DEFAULT_SOCK = "/tmp/hostd.sock";

int main(int argc, char **argv) {
    const char *sock = DEFAULT_SOCK;
    int argi = 1;
    if (argc >= 3 && strcmp(argv[1], "-S")==0) { sock = argv[2]; argi = 3; }
    if (argi >= argc) {
        fprintf(stderr, "Usage: %s [-S socket] COMMAND [ARGS...]\n", argv[0]);
        return 1;
    }

    // join args into a single command line
    size_t total = 0;
    for (int i=argi;i<argc;i++) total += strlen(argv[i]) + 1;
    char *line = malloc(total+2);
    line[0] = 0;
    for (int i=argi;i<argc;i++) {
        strcat(line, argv[i]);
        if (i+1<argc) strcat(line, " ");
    }
    strcat(line, "\n");

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    if (write(fd, line, strlen(line)) < 0) { perror("write"); return 1; }
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    if (n < 0) { perror("read"); return 1; }
    buf[n] = 0;
    fputs(buf, stdout);
    close(fd);
    free(line);
    return 0;
}
