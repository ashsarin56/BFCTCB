// event_loop.h - epoll-based event loop that accepts multiple client connections
#pragma once
#include "common/types.h"
class EventLoop {
public:
    EventLoop();
    ~EventLoop();
    bool init(uint16_t listen_port);
    void run();
    void shutdown();

private:
    fd_t epoll_fd_;
    fd_t listener_fd_;
    bool running_;
    void handle_accept();
};
