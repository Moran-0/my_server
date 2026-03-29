#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include <iostream>

#include "src/Server.h"
#include "src/EventLoop.h"
using std::cout;
using std::endl;
using std::vector;

int main()
{
    EventLoop *loop = new EventLoop();
    Server *server = new Server(loop);
    loop->loop();
    return 0;
}