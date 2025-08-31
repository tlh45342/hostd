#pragma once
#include <stddef.h>

// Parse a single line command and write a one-line response into outbuf.
// Returns number of bytes written (excluding terminating NUL), or -1 on error.
int protocol_handle_line(const char *line, char *outbuf, size_t outsz);
