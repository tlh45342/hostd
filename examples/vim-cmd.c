// vim-cmd.c - cross-platform client for hostd with config + interactive shell
// - Unix (Linux/macOS): UNIX domain sockets + TCP
// - Windows: TCP (Winsock2)
// Build (Unix):    cc -Wall -Wextra -O2 -g -o vim-cmd examples/vim-cmd.c
// Build (MinGW):   x86_64-w64-mingw32-gcc -O2 -o vim-cmd.exe examples/vim-cmd.c -lws2_32

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET socket_t;
  #define CLOSESOCK closesocket
  #define SOCKERR() WSAGetLastError()
  #define PATH_SEP '\\'
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <sys/un.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <pwd.h>
  #include <sys/stat.h>
  typedef int socket_t;
  #define CLOSESOCK close
  #define SOCKERR() errno
  #define PATH_SEP '/'
#endif

typedef enum { MODE_UNSET=0, MODE_UNIX=1, MODE_TCP=2 } vc_mode_t;

typedef struct {
    vc_mode_t mode;
    char   socket_path[256];
    char   host[128];
    int    port;
    char   cfg_path[512];
} cfg_t

static const char *DEFAULT_UNIX_SOCK =
#ifdef _WIN32
    NULL
#else
    "/tmp/hostd.sock"
#endif
;

static char *trim(char *s) {
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s==0) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = 0;
    return s;
}

static void default_cfg_path(char *out, size_t outsz) {
#ifdef _WIN32
    char *appdata = getenv("APPDATA"); // e.g., C:\Users\you\AppData\Roaming
    if (appdata && *appdata) {
        snprintf(out, outsz, "%s\\vim-cmd\\config", appdata);
    } else {
        // Fallback to USERPROFILE
        char *home = getenv("USERPROFILE");
        if (!home) home = "";
        snprintf(out, outsz, "%s\\AppData\\Roaming\\vim-cmd\\config", home);
    }
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(out, outsz, "%s/vim-cmd/config", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home || !*home) {
            struct passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "";
        }
        snprintf(out, outsz, "%s/.config/vim-cmd/config", home ? home : "");
    }
#endif
}

static void cfg_init_defaults(cfg_t *c) {
    memset(c, 0, sizeof(*c));
#ifdef _WIN32
    c->mode = MODE_TCP;
    c->port = 9000;
    strncpy(c->host, "127.0.0.1", sizeof(c->host)-1);
#else
    c->mode = MODE_UNIX;
    if (DEFAULT_UNIX_SOCK)
        strncpy(c->socket_path, DEFAULT_UNIX_SOCK, sizeof(c->socket_path)-1);
#endif
    default_cfg_path(c->cfg_path, sizeof(c->cfg_path));
}

static void cfg_show(const cfg_t *c) {
    fprintf(stderr, "[cfg] mode=%s socket=%s host=%s port=%d cfg=%s\n",
        c->mode==MODE_TCP?"tcp":(c->mode==MODE_UNIX?"unix":"unset"),
        c->socket_path[0]?c->socket_path:"(n/a)",
        c->host[0]?c->host:"(n/a)",
        c->port,
        c->cfg_path[0]?c->cfg_path:"(none)");
}

static void cfg_load_file(cfg_t *c, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return; // optional
    char line[512];
    while (fgets(line, sizeof line, fp)) {
        char *p = trim(line);
        if (*p=='#' || *p==0) continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(p);
        char *v = trim(eq+1);
        if (strcasecmp(k,"mode")==0) {
#ifdef _WIN32
            if (!strcasecmp(v,"tcp")) c->mode = MODE_TCP; // unix mode unsupported on Windows
#else
            if (!strcasecmp(v,"tcp")) c->mode = MODE_TCP;
            else if (!strcasecmp(v,"unix")) c->mode = MODE_UNIX;
#endif
        } else if (strcasecmp(k,"socket")==0) {
#ifndef _WIN32
            snprintf(c->socket_path, sizeof(c->socket_path), "%s", v);
#endif
        } else if (strcasecmp(k,"host")==0) {
            snprintf(c->host, sizeof(c->host), "%s", v);
        } else if (strcasecmp(k,"port")==0) {
            c->port = atoi(v);
        }
    }
    fclose(fp);
}

#ifndef _WIN32
static int connect_unix_path(const char *sock) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket(AF_UNIX)"); return -1; }
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect(unix)"); CLOSESOCK(fd); return -1;
    }
    return fd;
}
#endif

static int connect_tcp_host(const char *host, int port) {
    char portstr[16]; snprintf(portstr, sizeof portstr, "%d", port);
    struct addrinfo hints, *res=NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, portstr, &hints, &res);
    if (err) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); return -1; }

    socket_t fd = (socket_t)-1;
    for (struct addrinfo *ai=res; ai; ai=ai->ai_next) {
        fd = (socket_t)socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if ((int)fd < 0) continue;
        if (connect(fd, ai->ai_addr, (int)ai->ai_addrlen) == 0) { freeaddrinfo(res); return (int)fd; }
        CLOSESOCK(fd); fd = (socket_t)-1;
    }
    freeaddrinfo(res);
#ifdef _WIN32
    fprintf(stderr, "connect(tcp) failed, WSAErr=%d\n", SOCKERR());
#else
    perror("connect(tcp)");
#endif
    return -1;
}

static int connect_from_cfg(const cfg_t *c) {
    if (c->mode == MODE_TCP) {
        if (!c->host[0] || c->port<=0) { fprintf(stderr, "tcp config incomplete\n"); return -1; }
        return connect_tcp_host(c->host, c->port);
    }
#ifndef _WIN32
    if (c->mode == MODE_UNIX) {
        if (!c->socket_path[0]) { fprintf(stderr, "unix socket path missing\n"); return -1; }
        return connect_unix_path(c->socket_path);
    }
#endif
    fprintf(stderr, "no valid mode\n");
    return -1;
}

static int send_command_fd(int fd, const char *line) {
    size_t len = strlen(line);
    // append newline if needed
#ifdef _WIN32
    // server expects '\n'; use "\n"
#endif
    if (len==0 || line[len-1] != '\n') {
#ifdef _WIN32
        if (send(fd, line, (int)len, 0) < 0) return -1;
        if (send(fd, "\n", 1, 0) < 0) return -1;
#else
        if (write(fd, line, len) < 0) return -1;
        if (write(fd, "\n", 1) < 0) return -1;
#endif
    } else {
#ifdef _WIN32
        if (send(fd, line, (int)len, 0) < 0) return -1;
#else
        if (write(fd, line, len) < 0) return -1;
#endif
    }

    char buf[8192];
#ifdef _WIN32
    int n = recv(fd, buf, (int)sizeof(buf)-1, 0);
    if (n < 0) { fprintf(stderr, "recv failed, WSAErr=%d\n", SOCKERR()); return -1; }
#else
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    if (n < 0) { perror("read"); return -1; }
#endif
    if (n == 0) { fprintf(stderr, "server closed connection\n"); return -2; }
    buf[n] = 0;
    fputs(buf, stdout);
    return 0;
}

static void usage(const char *prog) {
#ifdef _WIN32
    fprintf(stderr,
        "Usage:\n"
        "  %s [-c cfgfile] [-T host:port] COMMAND [ARGS...]\n"
        "  %s [-c cfgfile] [-T host:port]\n"
        "Config (Windows): %%APPDATA%%\\vim-cmd\\config\n",
        prog, prog);
#else
    fprintf(stderr,
        "Usage:\n"
        "  %s [-c cfgfile] [-S socket] [-T host:port] COMMAND [ARGS...]\n"
        "  %s [-c cfgfile] [-S socket] [-T host:port]\n"
        "Config (Unix): $XDG_CONFIG_HOME/vim-cmd/config or ~/.config/vim-cmd/config\n",
        prog, prog);
#endif
}

int main(int argc, char **argv) {
#ifdef _WIN32
    WSADATA w;
    if (WSAStartup(MAKEWORD(2,2), &w) != 0) { fprintf(stderr, "WSAStartup failed\n"); return 1; }
#endif

    cfg_t cfg; cfg_init_defaults(&cfg);

    int opt;
    const char *cli_cfg = NULL;
    const char *cli_tcp = NULL;
#ifndef _WIN32
    const char *cli_sock = NULL;
#endif

    // very small getopt replacement for Windows (MSVC lacks unistd.h getopt)
#ifdef _WIN32
    for (int i=1; i<argc; ) {
        if (!strcmp(argv[i], "-c") && i+1<argc) { cli_cfg = argv[i+1]; i+=2; continue; }
        if (!strcmp(argv[i], "-T") && i+1<argc) { cli_tcp = argv[i+1]; i+=2; continue; }
        break;
    }
#else
    while ((opt = getopt(argc, argv, "c:S:T:h")) != -1) {
        switch (opt) {
            case 'c': cli_cfg = optarg; break;
            case 'S': cli_sock = optarg; break;
            case 'T': cli_tcp  = optarg; break;
            case 'h': default: usage(argv[0]); return opt=='h'?0:1;
        }
    }
#endif

    if (cli_cfg) {
        strncpy(cfg.cfg_path, cli_cfg, sizeof(cfg.cfg_path)-1);
        cfg_load_file(&cfg, cfg.cfg_path);
    } else {
        cfg_load_file(&cfg, cfg.cfg_path);
    }

#ifndef _WIN32
    if (cli_sock) {
        cfg.mode = MODE_UNIX;
        strncpy(cfg.socket_path, cli_sock, sizeof(cfg.socket_path)-1);
    }
#endif

    if (cli_tcp) {
        const char *colon = strchr(cli_tcp, ':');
        if (!colon) { fprintf(stderr, "-T expects host:port\n"); return 1; }
        size_t hl = (size_t)(colon - cli_tcp);
        if (hl >= sizeof(cfg.host)) { fprintf(stderr, "host too long\n"); return 1; }
        memcpy(cfg.host, cli_tcp, hl); cfg.host[hl]=0;
        cfg.port = atoi(colon+1);
        cfg.mode = MODE_TCP;
    }

#ifdef _WIN32
    int first_cmd = (cli_cfg || cli_tcp) ? 1 : 1; // default; simple parser
    // find first non-flag arg
    int argi = 1;
    while (argi < argc && argv[argi][0]=='-') {
        if ((argv[argi][1]=='c' || argv[argi][1]=='T') && argi+1<argc) argi+=2;
        else argi++;
    }
#else
    int argi = optind;
#endif

    // One-shot mode if a command was provided
    if (argi < argc) {
        size_t total=0; for (int i=argi;i<argc;i++) total += strlen(argv[i])+1;
        char *line = (char*)malloc(total+2); if (!line) { perror("malloc"); return 1; }
        line[0]=0; for (int i=argi;i<argc;i++){ strcat(line, argv[i]); if (i+1<argc) strcat(line," "); }
        int fd = connect_from_cfg(&cfg);
        if (fd < 0) { free(line); return 2; }
        int rc = send_command_fd(fd, line);
        CLOSESOCK(fd);
        free(line);
#ifdef _WIN32
        WSACleanup();
#endif
        return (rc==0)?0:3;
    }

    // Interactive shell
    fprintf(stderr, "vim-cmd shell. Type /help. Using config: %s\n", cfg.cfg_path);
reconnect:
    cfg_show(&cfg);
    int fd = connect_from_cfg(&cfg);
    if (fd < 0) {
        fprintf(stderr, "unable to connect; you can /connect or Ctrl-C\n");
    }

    char *line = NULL; size_t cap=0;
    // portable getline: use POSIX getline if available; otherwise minimal fallback
#if defined(_WIN32)
    // Simple fgets-based loop
    char buf[4096];
    for (;;) {
        fprintf(stderr, "vim-cmd> ");
        if (!fgets(buf, sizeof buf, stdin)) { fprintf(stderr, "\n"); break; }
        char *cmd = trim(buf);
#else
    for (;;) {
        fprintf(stderr, "vim-cmd> ");
        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0) { fprintf(stderr, "\n"); break; }
        char *cmd = trim(line);
#endif
        if (*cmd==0) continue;

        if (cmd[0]=='/') {
            if (!strcasecmp(cmd,"/quit") || !strcasecmp(cmd,"/exit")) break;
            if (!strcasecmp(cmd,"/help")) {
#ifdef _WIN32
                fprintf(stderr, "Built-ins:\n  /help\n  /show\n  /connect tcp <host> <port>\n  /quit\n");
#else
                fprintf(stderr, "Built-ins:\n  /help\n  /show\n  /connect tcp <host> <port>\n  /connect unix <socket>\n  /quit\n");
#endif
                continue;
            }
            if (!strcasecmp(cmd,"/show")) { cfg_show(&cfg); continue; }
            if (!strncasecmp(cmd,"/connect",8)) {
                char kind[16]={0}, a[256]={0}, b[64]={0};
                int n = sscanf(cmd+8, "%15s %255s %63s", kind, a, b);
                if (n >= 2) {
                    if (!strcasecmp(kind,"tcp")) {
                        int p = (n>=3)?atoi(b):cfg.port;
                        if (p<=0) { fprintf(stderr, "bad port\n"); continue; }
                        cfg.mode = MODE_TCP;
                        strncpy(cfg.host, a, sizeof(cfg.host)-1);
                        cfg.port = p;
#ifndef _WIN32
                    } else if (!strcasecmp(kind,"unix")) {
                        cfg.mode = MODE_UNIX;
                        strncpy(cfg.socket_path, a, sizeof(cfg.socket_path)-1);
#endif
                    } else {
#ifdef _WIN32
                        fprintf(stderr, "usage: /connect tcp <host> <port>\n");
#else
                        fprintf(stderr, "usage: /connect tcp <host> <port> | /connect unix <socket>\n");
#endif
                        continue;
                    }
                    if (fd >= 0) { CLOSESOCK(fd); fd = -1; }
                    goto reconnect;
                } else {
#ifdef _WIN32
                    fprintf(stderr, "usage: /connect tcp <host> <port>\n");
#else
                    fprintf(stderr, "usage: /connect tcp <host> <port> | /connect unix <socket>\n");
#endif
                }
                continue;
            }
            fprintf(stderr, "unknown builtin; try /help\n");
            continue;
        }

        if (fd < 0) { fprintf(stderr, "not connected; try /connect\n"); continue; }
        int rc = send_command_fd(fd, cmd);
        if (rc == -2) {
            CLOSESOCK(fd); fd=-1;
            fprintf(stderr, "[info] reconnecting...\n");
            fd = connect_from_cfg(&cfg);
            if (fd < 0) fprintf(stderr, "reconnect failed.\n");
        }
    }

    if (fd >= 0) CLOSESOCK(fd);
#if !defined(_WIN32)
    free(line);
#endif

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
