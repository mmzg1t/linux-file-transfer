#include "common.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const char *host = NULL, *path = NULL;
    uint16_t port = 0;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc) goto usage;
        if (strcmp(argv[i], "--host") == 0) host = argv[i + 1];
        else if (strcmp(argv[i], "--file") == 0) path = argv[i + 1];
        else if (strcmp(argv[i], "--port") == 0) {
            if (parse_port(argv[i + 1], &port) < 0) goto usage;
        } else goto usage;
    }
    if (!host || !path || !port) goto usage;

    /* 1. Open the file and get its length. Do not change it during upload. */
    FILE *input = fopen(path, "rb");
    if (!input) { perror("fopen"); return 1; }
    int fd = -1, result = 1;
    struct stat st;
    if (fstat(fileno(input), &st) < 0) { perror("fstat"); goto cleanup; }
    if (!S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uintmax_t)st.st_size > MAX_FILE_SIZE) {
        fprintf(stderr, "Expected a regular file no larger than 64 MiB\n");
        goto cleanup;
    }

    /* 2. Create an IPv4 TCP socket and connect to the server. */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); goto cleanup; }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "--host must be an IPv4 address\n");
        goto cleanup;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); goto cleanup;
    }

    /* 3. Send a 4-byte length in network byte order, then file bytes. */
    uint32_t remaining = (uint32_t)st.st_size;
    uint32_t wire_size = htonl(remaining);
    if (send_all(fd, &wire_size, sizeof(wire_size)) < 0) {
        perror("send length"); goto cleanup;
    }
    unsigned char buffer[CHUNK_SIZE];
    while (remaining > 0) {
        size_t want = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        size_t n = fread(buffer, 1, want, input);
        if (n == 0) {
            fprintf(stderr, "File read failed or file became shorter\n");
            goto cleanup;
        }
        if (send_all(fd, buffer, n) < 0) { perror("send data"); goto cleanup; }
        remaining -= (uint32_t)n;
    }

    /* 4. Sending bytes is not proof that the server saved the file. */
    unsigned char status;
    if (recv_exact(fd, &status, 1) < 0) { perror("receive result"); goto cleanup; }
    if (status != 0) {
        fprintf(stderr, "Server could not save the file\n"); goto cleanup;
    }
    printf("Upload successful: %s (%ju bytes)\n", path, (uintmax_t)st.st_size);
    result = 0;

cleanup:
    if (fd >= 0) close(fd);
    fclose(input);
    return result;

usage:
    fprintf(stderr, "Usage: %s --host 127.0.0.1 --port 9000 --file example.txt\n", argv[0]);
    return 1;
}
