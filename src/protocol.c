// protocol.c - parse and dispatch commands
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "protocol.h"
#include "libvm.h"
#include "hostd.h"
#include "log.h"

static int ok(char *out, size_t outsz, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = snprintf(out, outsz, "200 OK ");
    n += vsnprintf(out+n, outsz-n, fmt, ap);
    va_end(ap);
    if ((size_t)n < outsz-1) { out[n++] = '\n'; out[n] = 0; }
    return n;
}
static int err(char *out, size_t outsz, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = snprintf(out, outsz, "400 ERR ");
    n += vsnprintf(out+n, outsz-n, fmt, ap);
    va_end(ap);
    if ((size_t)n < outsz-1) { out[n++] = '\n'; out[n] = 0; }
    return n;
}

// simple kv parser: key=val tokens, whitespace separated
static const char* kv_get(const char *key, char *copy, size_t copy_sz) {
    // search token "key=" in copy
    size_t keylen = strlen(key);
    char *p = copy;
    while (p && *p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char *end = p;
        while (*end && !isspace((unsigned char)*end)) end++;
        char save = *end; *end = 0;
        if (strncmp(p, key, keylen)==0 && p[keylen]=='=') {
            char *val = p + keylen + 1;
            // strip quotes if present
            if (*val=='\"') {
                val++;
                char *q = strrchr(val, '\"');
                if (q) *q=0;
            }
            return val;
        }
        *end = save;
        p = end;
    }
    return NULL;
}

int protocol_handle_line(const char *line, char *outbuf, size_t outsz) {
    // make a writable copy
    char tmp[1024];
    strncpy(tmp, line, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = 0;

    // uppercase command word
    char *cmd = tmp;
    while (*cmd && isspace((unsigned char)*cmd)) cmd++;
    char *sp = cmd;
    while (*sp && !isspace((unsigned char)*sp)) { *sp = toupper((unsigned char)*sp); sp++; }
    char *rest = sp;
    while (*rest && isspace((unsigned char)*rest)) rest++;

    if (strcmp(cmd, "PING")==0) {
        return ok(outbuf, outsz, "PONG");
    } else if (strcmp(cmd, "VERSION")==0) {
        return ok(outbuf, outsz, "hostd " HOSTD_VERSION);
    } else if (strcmp(cmd, "HEALTH")==0) {
        return ok(outbuf, outsz, "healthy");
    } else if (strcmp(cmd, "ECHO")==0) {
        return ok(outbuf, outsz, "%s", rest);
    } else if (strcmp(cmd, "SHUTDOWN")==0) {
        g_running = 0;
        return ok(outbuf, outsz, "bye");
    } else if (strcmp(cmd, "VM.LIST")==0) {
        size_t count=0;
        vm_list(NULL, 0, &count);
        if (count==0) return ok(outbuf, outsz, "0 vms");
        // else list them
        vm_t arr[64];
        if (count > 64) count = 64;
        size_t got=0; vm_list(arr, count, &got);
        int n = snprintf(outbuf, outsz, "200 OK %zu vms", got);
        for (size_t i=0;i<got;i++) {
            n += snprintf(outbuf+n, outsz-n, " | id=%d name=%s mem=%d state=%s",
                          arr[i].id, arr[i].name, arr[i].mem_mib, arr[i].state);
        }
        if ((size_t)n < outsz-1) { outbuf[n++] = '\n'; outbuf[n]=0; }
        return n;
    } else if (strcmp(cmd, "VM.CREATE")==0) {
        char copy[512]; strncpy(copy, rest, sizeof(copy)-1); copy[sizeof(copy)-1]=0;
        const char *name = kv_get("name", copy, sizeof(copy));
        const char *mems = kv_get("mem", copy, sizeof(copy));
        if (!name || !mems) return err(outbuf, outsz, "missing name= or mem=");
        int mem = atoi(mems);
        int id=0;
        int rc = vm_create(name, mem, &id);
        if (rc!=0) return err(outbuf, outsz, "vm_create failed (%d)", rc);
        return ok(outbuf, outsz, "id=%d", id);
    } else if (strcmp(cmd, "VM.INFO")==0) {
        char copy[512]; strncpy(copy, rest, sizeof(copy)-1); copy[sizeof(copy)-1]=0;
        const char *ids = kv_get("id", copy, sizeof(copy));
        if (!ids) return err(outbuf, outsz, "missing id=");
        int id = atoi(ids);
        vm_t v;
        int rc = vm_info(id, &v);
        if (rc!=0) return err(outbuf, outsz, "not found");
        return ok(outbuf, outsz, "id=%d name=%s mem=%d state=%s", v.id, v.name, v.mem_mib, v.state);
    } else if (strcmp(cmd, "VM.DESTROY")==0) {
        char copy[512]; strncpy(copy, rest, sizeof(copy)-1); copy[sizeof(copy)-1]=0;
        const char *ids = kv_get("id", copy, sizeof(copy));
        if (!ids) return err(outbuf, outsz, "missing id=");
        int id = atoi(ids);
        int rc = vm_destroy(id);
        if (rc!=0) return err(outbuf, outsz, "not found");
        return ok(outbuf, outsz, "destroyed id=%d", id);
    }

    return err(outbuf, outsz, "unknown command");
}
