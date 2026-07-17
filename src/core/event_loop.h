// event_loop.h - epoll-based event loop with connection tracking for client-backend pairs
#pragma once
#include "common/types.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct Connection {
    fd_t client_fd;
    fd_t backend_fd;
    std::vector<uint8_t> c2b_buf;
    std::vector<uint8_t> b2c_buf;
};

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
    std::unordered_map<fd_t, std::shared_ptr<Connection>> connections_;

    void handle_accept();
    void remove_connection(fd_t fd);
};
