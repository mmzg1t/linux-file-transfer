#include "common.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>

int send_all(int fd, const void *buffer, size_t length)
{
    const unsigned char *p = buffer;
    size_t done = 0;
    while (done < length) {
        ssize_t n = send(fd, p + done, length - done, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) { errno = EPIPE; return -1; }
        done += (size_t)n;
    }
    return 0;
}

int recv_exact(int fd, void *buffer, size_t length)
{
    unsigned char *p = buffer;
    size_t done = 0;
    while (done < length) {
        ssize_t n = recv(fd, p + done, length - done, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        /* EOF before the expected length means an incomplete message. */
        if (n == 0) { errno = ECONNRESET; return -1; }
        done += (size_t)n;
    }
    return 0;
}

int parse_port(const char *text, uint16_t *port)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || value < 1 || value > 65535)
        return -1;
    *port = (uint16_t)value;
    return 0;
}

