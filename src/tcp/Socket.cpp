#include "Socket.h"
#include "InetAddress.h"
#include "Logging.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

Socket::Socket() : sockfd(-1)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建TCP套接字
    if (sockfd == -1) {
        LOG_ERROR << "Failed to create socket";
    }
    int opt = 1;
    // 将time-wait状态下的套接字端口号重新分配给新的套接字，跳过time-wait的等待阶段
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        LOG_ERROR << "Failed to set SO_REUSEADDR";
    }
}
Socket::Socket(int fd) : sockfd(fd) {
    if (sockfd == -1) {
        LOG_ERROR << "Invalid socket file descriptor";
    }
}
Socket::~Socket()
{
    if (sockfd != -1)
    {
        close(sockfd);
        sockfd = -1;
    }
}
void Socket::bind(const InetAddress& addr)
{
    auto sock_addr = addr.getAddr();
    if (::bind(sockfd, (sockaddr*)&sock_addr, addr.getAddrLen()) == -1) {
        LOG_ERROR << "Failed to bind socket";
    }
}
void Socket::listen()
{
    if (::listen(sockfd, SOMAXCONN) == -1) {
        LOG_ERROR << "Failed to listen on socket";
    }
}
// 设置非阻塞io
void Socket::setNonBlocking()
{
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
}
int Socket::accept(InetAddress& addr)
{
    sockaddr_in sock_addr = addr.getAddr();
    socklen_t addr_len = addr.getAddrLen();
    int client_fd = ::accept(sockfd, (sockaddr*)&sock_addr, &addr_len);
    addr.setAddr(sock_addr);
    addr.setAddrLen(addr_len);
    return client_fd;
}

void Socket::connect(const InetAddress& addr)
{
    auto sock_addr = addr.getAddr();
    if (::connect(sockfd, (sockaddr*)&sock_addr, addr.getAddrLen()) == -1) {
        LOG_ERROR << "Failed to connect to server";
    }
}
