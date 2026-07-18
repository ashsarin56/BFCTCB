// event_loop.h - epoll-based event loop with multi-port routing via Router
#pragma once
#include "common/types.h"
#include "routing/router.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

struct Connection {
    fd_t client_fd;
    fd_t backend_fd;
    fd_t pipe_c2b[2];
    fd_t pipe_b2c[2];
    size_t c2b_pipe_bytes;
    size_t b2c_pipe_bytes;
};

class EventLoop {
public:
    EventLoop();
    ~EventLoop();
    bool init(const Router& router);
    void run();
    void shutdown();

private:
    fd_t epoll_fd_;
    std::atomic<bool> running_;
    std::unordered_map<fd_t, uint16_t> listeners_;
    const Router* router_;
    std::unordered_map<fd_t, std::shared_ptr<Connection>> connections_;

    void handle_accept(fd_t listener_fd);
    void handle_read(fd_t fd);
    void handle_write(fd_t fd);
    void handle_disconnect(fd_t fd);
    void remove_connection(fd_t fd);
};
