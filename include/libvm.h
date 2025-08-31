#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   id;
    char  name[64];
    int   mem_mib;
    char  state[16]; // "stopped", "running", etc.
} vm_t;

// Initialize/shutdown VM subsystem (stub impl here)
int vm_init(void);
int vm_shutdown(void);

// List VMs. If out==NULL, only count is returned in *count.
int vm_list(vm_t *out, size_t max, size_t *count);

// Create/destroy/info
int vm_create(const char *name, int mem_mib, int *out_id);
int vm_destroy(int id);
int vm_info(int id, vm_t *out);

#ifdef __cplusplus
}
#endif
