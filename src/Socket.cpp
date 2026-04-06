#include "Socket.h"
#include "InetAddress.h"
#include "Util.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

Socket::Socket() : sockfd(-1)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建TCP套接字
    errif(sockfd == -1, "Failed to create socket");
    int opt = 1;
    errif(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1, "Failed to set SO_REUSEADDR"); // 将time-wait状态下的套接字端口号重新分配给新的套接字，跳过time-wait的等待阶段
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
    auto sock_addr = addr.getAddr();
    errif(::bind(sockfd, (sockaddr *)&sock_addr, addr.getAddrLen()) == -1, "Failed to bind socket");
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
    auto sock_addr = addr->getAddr();
    auto addr_len = addr->getAddrLen();
    int client_fd = ::accept(sockfd, (sockaddr *)&sock_addr, &addr_len);
    return client_fd;
}

void Socket::connect(const InetAddress &addr)
{
    auto sock_addr = addr.getAddr();
    errif(::connect(sockfd, (sockaddr *)&sock_addr, addr.getAddrLen()) == -1, "connect failed!");
}
