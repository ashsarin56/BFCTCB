// event_loop.cpp - epoll event loop: registers listener, accepts multiple clients, logs and closes each
#include "event_loop.h"
#include "socket_utils.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
static constexpr int MAX_EVENTS = 64;

EventLoop::EventLoop()
    : epoll_fd_(INVALID_FD), listener_fd_(INVALID_FD), running_(false) {}
EventLoop::~EventLoop() {
    if (epoll_fd_ != INVALID_FD) close_fd(epoll_fd_);
    if (listener_fd_ != INVALID_FD) close_fd(listener_fd_);
}
bool EventLoop::init(uint16_t listen_port) {
    listener_fd_ = create_listener(listen_port);
    if (listener_fd_ == INVALID_FD) {
        std::fprintf(stderr, "event_loop: failed to create listener on port %u\n", listen_port);
        return false;
    }
    if (!set_nonblocking(listener_fd_)) {
        std::fprintf(stderr, "event_loop: failed to set listener non-blocking\n");
        close_fd(listener_fd_);
        listener_fd_ = INVALID_FD;
        return false;
    }
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == INVALID_FD) {
        std::fprintf(stderr, "event_loop: epoll_create1 failed: %s\n", std::strerror(errno));
        close_fd(listener_fd_);
        listener_fd_ = INVALID_FD;
        return false;
    }
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listener_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listener_fd_, &ev) < 0) {
        std::fprintf(stderr, "event_loop: epoll_ctl add listener failed: %s\n", std::strerror(errno));
        close_fd(epoll_fd_);
        close_fd(listener_fd_);
        epoll_fd_ = INVALID_FD;
        listener_fd_ = INVALID_FD;
        return false;
    }
    std::fprintf(stdout, "gateway listening on port %u...\n", listen_port);
    std::fflush(stdout);
    return true;
}
void EventLoop::run() {
    running_ = true;
    struct epoll_event events[MAX_EVENTS];
    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR)continue;
            std::fprintf(stderr, "event_loop: epoll_wait failed: %s\n", std::strerror(errno));
            break;
        }
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == listener_fd_)handle_accept();
        }
    }
}
void EventLoop::shutdown() {
    running_ = false;
}
void EventLoop::handle_accept() {
    while (true) {
        std::string client_ip;
        fd_t client_fd = accept_client(listener_fd_, client_ip);
        if (client_fd == INVALID_FD) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)break;
            std::fprintf(stderr, "event_loop: accept_client failed: %s\n", std::strerror(errno));
            break;
        }
        std::fprintf(stdout, "client connected: %s\n", client_ip.c_str());
        std::fflush(stdout);
        close_fd(client_fd);
    }
}
