#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

#define MAX_FILE_SIZE (64U * 1024U * 1024U)
#define CHUNK_SIZE 4096

/* These helpers are for blocking sockets. 0 = success, -1 = failure. */
int send_all(int fd, const void *buffer, size_t length);
int recv_exact(int fd, void *buffer, size_t length);
int parse_port(const char *text, uint16_t *port);

#endif
