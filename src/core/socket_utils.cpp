// socket_utils.cpp - TCP socket helper implementations
#include "socket_utils.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

fd_t create_listener(uint16_t port) {
    fd_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_FD) {
        std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
        return INVALID_FD;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno) << "\n";
        close(sock);
        return INVALID_FD;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed on port " << port << ": " << std::strerror(errno) << "\n";
        close(sock);
        return INVALID_FD;
    }

    if (listen(sock, 128) < 0) {
        std::cerr << "listen() failed: " << std::strerror(errno) << "\n";
        close(sock);
        return INVALID_FD;
    }

    return sock;
}

fd_t accept_client(fd_t listener_fd, std::string& client_ip_out) {
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);

    fd_t client_fd = accept(listener_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
        std::cerr << "accept() failed: " << std::strerror(errno) << "\n";
        return INVALID_FD;
    }

    char ip_buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf)) == nullptr) {
        std::cerr << "inet_ntop() failed: " << std::strerror(errno) << "\n";
        client_ip_out = "unknown";
    } else {
        client_ip_out = ip_buf;
    }

    return client_fd;
}

bool set_nonblocking(fd_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        std::cerr << "fcntl(F_GETFL) failed: " << std::strerror(errno) << "\n";
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "fcntl(F_SETFL) failed: " << std::strerror(errno) << "\n";
        return false;
    }

    return true;
}

void close_fd(fd_t fd) {
    if (fd == INVALID_FD) {
        return;
    }

    while (close(fd) < 0) {
        if (errno != EINTR) {
            break;
        }
    }
}
