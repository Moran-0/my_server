#include "InetAddress.h"
#include "Util.h"
#include <string>
#include <cstring>

InetAddress::InetAddress() : addr_len(sizeof(addr))
{
    memset(&addr, 0, sizeof(addr));
}
InetAddress::InetAddress(const char *ip, uint16_t port) : addr_len(sizeof(addr))
{
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    errif(inet_pton(AF_INET, ip, &addr.sin_addr) <= 0, "Invalid IP address");
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr_len = sizeof(addr);
}

sockaddr_in InetAddress::getAddr() const
{
    return addr;
}

socklen_t InetAddress::getAddrLen() const
{
    return addr_len;
}
