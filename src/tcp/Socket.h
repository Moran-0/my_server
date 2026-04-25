#pragma once
class InetAddress;
class Socket
{
private:
    int sockfd;

public:
    Socket();
    Socket(int fd);
    ~Socket();
    void bind(const InetAddress &addr);
    void listen();
    int accept(InetAddress& addr);
    void connect(const InetAddress &addr);
    void setNonBlocking();
    int getFd() const { return sockfd; }
};