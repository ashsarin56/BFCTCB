#include "health_checker.h"
#include "common/types.h"
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>

static constexpr int MAX_EVENTS = 16;

HealthChecker::HealthChecker(const Router& router, int interval_seconds)
    : router_(router), interval_seconds_(interval_seconds), epoll_fd_(INVALID_FD), timer_fd_(INVALID_FD), running_(false) {}

HealthChecker::~HealthChecker() {
    if (timer_fd_ != INVALID_FD) {
        close(timer_fd_);
    }
    if (epoll_fd_ != INVALID_FD) {
        close(epoll_fd_);
    }
}

bool HealthChecker::init() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == INVALID_FD) {
        return false;
    }

    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd_ == INVALID_FD) {
        return false;
    }

    struct itimerspec ts{};
    ts.it_value.tv_sec = interval_seconds_;
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = interval_seconds_;
    ts.it_interval.tv_nsec = 0;
    
    if (timerfd_settime(timer_fd_, 0, &ts, nullptr) < 0) {
        return false;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = timer_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &ev) < 0) {
        return false;
    }

    return true;
}

void HealthChecker::run() {
    running_ = true;
    struct epoll_event events[MAX_EVENTS];
    while (running_.load()) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == timer_fd_) {
                uint64_t expirations;
                if (read(timer_fd_, &expirations, sizeof(expirations)) > 0) {
                    check_unhealthy_backends();
                }
            }
        }
    }
}

void HealthChecker::stop() {
    running_.store(false);
}

void HealthChecker::check_unhealthy_backends() {
    for (const auto& pair : router_.get_routes()) {
        const ServiceTarget& target = pair.second;
        for (const auto& backend : target.backends) {
            if (!backend->is_healthy.load()) {
                if (try_connect(backend->host, backend->port)) {
                    backend->is_healthy.store(true);
                    std::fprintf(stderr, "backend %s:%u recovered, marking healthy\n", backend->host.c_str(), backend->port);
                }
            }
        }
    }
}

bool HealthChecker::try_connect(const std::string& host, uint16_t port) {
    fd_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_FD) {
        return false;
    }

    struct timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        close(sock);
        return false;
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
        close(sock);
        return true;
    }
    
    close(sock);
    return false;
}
