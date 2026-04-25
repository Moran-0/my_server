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
    void setAddr(const sockaddr_in& addr_);
    void setAddrLen(socklen_t len_);

  private:
    sockaddr_in addr;
    socklen_t addr_len;
};