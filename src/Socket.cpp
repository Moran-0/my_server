#include "Socket.h"
#include "InetAddress.h"
#include "Util.h"
#include <unistd.h>
#include <fcntl.h>

Socket::Socket() : sockfd(-1)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建TCP套接字
    errif(sockfd == -1, "Failed to create socket");
}
Socket::Socket(int fd) : sockfd(fd)
{
    errif(sockfd == -1, "Invalid socket file descriptor");
}
Socket::~Socket()
{
    if (sockfd != -1)
    {
        close(sockfd);
        sockfd = -1;
    }
}
void Socket::bind(const InetAddress &addr)
{
    errif(::bind(sockfd, (sockaddr *)&addr.addr, addr.addr_len) == -1, "Failed to bind socket");
}
void Socket::listen()
{
    errif(::listen(sockfd, SOMAXCONN) == -1, "Failed to listen on socket");
}
// 设置非阻塞io
void Socket::setNonBlocking()
{
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
}
int Socket::accept(InetAddress *addr)
{
    int client_fd = ::accept(sockfd, (sockaddr *)&addr->addr, &addr->addr_len);
    errif(client_fd == -1, "Failed to accept connection");
    return client_fd;
}
