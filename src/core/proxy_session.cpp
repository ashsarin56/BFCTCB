// proxy_session.cpp - one-way forwarding loop: reads from source fd, writes to sink fd
#include "proxy_session.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "common/types.h"
void forward_one_way(fd_t from_fd, fd_t to_fd) {
    uint8_t buf[BUFFER_SIZE];
    while (true) {
        ssize_t n = read(from_fd, buf, sizeof(buf));
        if (n == 0)break;
        if (n < 0) {
            if (errno == EINTR)continue;
            std::fprintf(stderr, "read() failed: %s\n", std::strerror(errno));
            break;
        }
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(to_fd, buf + written, static_cast<size_t>(n - written));
            if (w < 0) {
                if (errno == EINTR)continue;
                std::fprintf(stderr, "write() failed: %s\n", std::strerror(errno));
                return;
            }
            written += w;
        }
    }
}
