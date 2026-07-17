// proxy_session.h - proxy session struct and bidirectional forwarding declaration
#pragma once
#include "common/types.h"
struct ProxySession {
    fd_t client_fd;
    fd_t backend_fd;
};
// reads bytes bidirectionally using select.
void run_bidirectional(ProxySession& session);
