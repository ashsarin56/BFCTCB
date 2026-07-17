// proxy_session.h - ProxySession struct and one-way forwarding declaration
#pragma once
#include "common/types.h"
struct ProxySession {
    fd_t client_fd;
    fd_t backend_fd;
};
// reads bytes from from_fd and writes them to to_fd in a blocking loop.
void forward_one_way(fd_t from_fd, fd_t to_fd);
