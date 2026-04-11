#pragma once
#include <arpa/inet.h>

class InetAddress
{
public:
    InetAddress();
    ~InetAddress() = default;
    InetAddress(const char *ip, uint16_t port);
    sockaddr_in getAddr() const;
    socklen_t getAddrLen() const;

private:
    sockaddr_in addr;
    socklen_t addr_len;
};