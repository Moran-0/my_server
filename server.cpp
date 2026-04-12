#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <vector>
#include <iostream>

#include "EventLoop.h"
#include "Server.h"
#include "pine.h"
using std::cout;
using std::endl;
using std::vector;

int main()
{
    auto* server = new Server();
    server->start();
    delete server;
    server = nullptr;
    return 0;
}
