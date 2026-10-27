#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int receive_file(int client_fd)
{
    /* 1. Read the complete length field before interpreting it. */
    uint32_t wire_size;
    if (recv_exact(client_fd, &wire_size, sizeof(wire_size)) < 0) {
        perror("receive length"); return -1;
    }
    uint32_t remaining = ntohl(wire_size);
    uint32_t total = remaining;
    if (remaining > MAX_FILE_SIZE) {
        fprintf(stderr, "File exceeds the 64 MiB limit\n"); return -1;
    }

    /* 2. Use a temporary file; interrupted uploads are not published. */
    char temporary[] = ".upload-XXXXXX";
    int output_fd = mkstemp(temporary);
    if (output_fd < 0) { perror("mkstemp"); return -1; }
    FILE *output = fdopen(output_fd, "wb");
    if (!output) {
        perror("fdopen"); close(output_fd); unlink(temporary); return -1;
    }

    int result = -1;
    unsigned char buffer[CHUNK_SIZE];
    while (remaining > 0) {
        size_t want = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (recv_exact(client_fd, buffer, want) < 0) {
            perror("receive data"); goto cleanup;
        }
        if (fwrite(buffer, 1, want, output) != want) {
            perror("write file"); goto cleanup;
        }
        remaining -= (uint32_t)want;
    }

    /* fclose also checks whether buffered writes succeeded. */
    int close_result = fclose(output);
    output = NULL;
    if (close_result != 0) { perror("close file"); goto cleanup; }
    /* Successful uploads replace the previous received.bin. */
    if (rename(temporary, "received.bin") < 0) {
        perror("rename"); goto cleanup;
    }
    printf("Saved received.bin (%u bytes)\n", (unsigned)total);
    result = 0;

cleanup:
    if (output) fclose(output);
    if (result != 0) unlink(temporary);
    return result;
}

int main(int argc, char **argv)
{
    uint16_t port = 0;
    const char *directory = NULL;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 == argc) goto usage;
        if (strcmp(argv[i], "--dir") == 0) directory = argv[i + 1];
        else if (strcmp(argv[i], "--port") == 0) {
            if (parse_port(argv[i + 1], &port) < 0) goto usage;
        } else goto usage;
    }
    if (!directory || !port) goto usage;
    if (chdir(directory) < 0) { perror("open upload directory"); return 1; }

    /* Create the listening socket on all local IPv4 interfaces. */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt"); close(listen_fd); return 1;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(listen_fd); return 1;
    }
    if (listen(listen_fd, 8) < 0) {
        perror("listen"); close(listen_fd); return 1;
    }
    printf("Listening on 0.0.0.0:%u; Ctrl+C to stop\n", (unsigned)port);

    /* 4. Accept and handle one connection at a time. */
    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept"); close(listen_fd); return 1;
        }
        unsigned char status = receive_file(client_fd) == 0 ? 0 : 1;
        if (send_all(client_fd, &status, 1) < 0) perror("send result");
        close(client_fd);
    }

usage:
    fprintf(stderr, "Usage: %s --port 9000 --dir ./uploads\n", argv[0]);
    return 1;
}
