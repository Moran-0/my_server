#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <netdb.h>

using std::cerr;
using std::cout;
using std::endl;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage" << argv[0] << "[IP/NAME]" << "<content>" << std::endl;
        exit(1);
    }
    if (strcmp(argv[1], "NAME") == 0)
    {
        if (argc == 4 && std::stoi(argv[3]) == 0)
        {
            auto addr = gethostbyname(argv[2]);
            if (addr == nullptr)
            {
                cerr << "gethostbyname error!" << endl;
                exit(1);
            }
            cout << "Official name:" << addr->h_name << endl;
            auto addr_list = addr->h_addr_list;
            for (auto p = addr_list; *p != nullptr; p++)
            {
                cout << "IP:" << inet_ntoa(*(in_addr *)*p) << endl;
            }
        }
        else
        {
            addrinfo hints, *res, *p;
            // 设置查询条件
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            int status = getaddrinfo(argv[2], nullptr, &hints, &res);
            if (status != 0)
            {
                std::cerr << "getaddrinfo error!" << std::endl;
                exit(1);
            }
            for (p = res; p != nullptr; p = p->ai_next)
            {
                char ip[INET_ADDRSTRLEN];
                auto addr = (sockaddr_in *)p->ai_addr;
                inet_ntop(AF_INET, &addr->sin_addr.s_addr, ip, sizeof(ip));
                cout << "The IP of " << argv[2] << "is " << ip << endl;
            }
            freeaddrinfo(res);
        }
    }
    else if (strcmp(argv[1], "IP") == 0)
    {
        if (argc == 4 && std::stoi(argv[3]) == 0)
        {
            in_addr addr;
            inet_aton(argv[2], &addr);
            auto host = gethostbyaddr(&addr, sizeof(addr), AF_INET);
            if (host == nullptr)
            {
                cerr << "gethostbyaddr error!" << endl;
                exit(1);
            }
            cout << "hostname is:" << host->h_name << endl;
        }
        else
        {
            sockaddr_in addr;
            addr.sin_family = AF_INET;
            inet_pton(AF_INET, argv[2], &addr.sin_addr.s_addr);
            // 接收数据
            char host[NI_MAXHOST];
            char service[NI_MAXSERV];
            auto addr_host = getnameinfo((sockaddr *)&addr, sizeof(addr), host, sizeof(host), service, sizeof(service), 0);
            cout << "host" << host << endl;
            cout << "service" << service << endl;
        }
    }
    else
    {
        std::cerr << "Please input 'IP' or 'NAME' option!" << std::endl;
        exit(1);
    }
    return 0;
}