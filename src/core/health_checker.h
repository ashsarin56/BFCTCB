#pragma once
#include "routing/router.h"
#include <atomic>

class HealthChecker {
public:
    HealthChecker(const Router& router, int interval_seconds);
    ~HealthChecker();
    bool init();
    void run();
    void stop();

private:
    const Router& router_;
    int interval_seconds_;
    int epoll_fd_;
    int timer_fd_;
    std::atomic<bool> running_;

    void check_unhealthy_backends();
    bool try_connect(const std::string& host, uint16_t port);
};
