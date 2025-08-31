// libvm_stub.c - in-memory stub impl for libvm
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libvm.h"

#define MAX_VMS 128

static vm_t vms[MAX_VMS];
static size_t vcount = 0;
static int next_id = 1;

int vm_init(void) {
    vcount = 0;
    next_id = 1;
    return 0;
}

int vm_shutdown(void) {
    vcount = 0;
    return 0;
}

int vm_list(vm_t *out, size_t max, size_t *count) {
    if (count) *count = vcount;
    if (!out) return 0;
    size_t n = (vcount < max) ? vcount : max;
    for (size_t i=0;i<n;i++) out[i] = vms[i];
    return 0;
}

static int find_index(int id) {
    for (size_t i=0;i<vcount;i++) if (vms[i].id == id) return (int)i;
    return -1;
}

int vm_create(const char *name, int mem_mib, int *out_id) {
    if (vcount >= MAX_VMS) return -1;
    vm_t v = {0};
    v.id = next_id++;
    snprintf(v.name, sizeof(v.name), "%s", name?name:"vm");
    v.mem_mib = mem_mib>0?mem_mib:512;
    snprintf(v.state, sizeof(v.state), "%s", "stopped");
    vms[vcount++] = v;
    if (out_id) *out_id = v.id;
    return 0;
}

int vm_destroy(int id) {
    int idx = find_index(id);
    if (idx < 0) return -1;
    // compact
    for (size_t i=idx+1;i<vcount;i++) vms[i-1] = vms[i];
    vcount--;
    return 0;
}

int vm_info(int id, vm_t *out) {
    int idx = find_index(id);
    if (idx < 0) return -1;
    if (out) *out = vms[idx];
    return 0;
}
