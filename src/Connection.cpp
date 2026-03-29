#include "Connection.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <iostream>
#include <string.h>
#include <unistd.h>

using std::cout;
using std::endl;

#define READ_BUFFER 1024

Connection::Connection(EventLoop *_loop, Socket *_sock) : loop(_loop), sock(_sock)
{
    sock->setNonBlocking();
    clnt_ch = new Channel(loop, sock->getFd());
    std::function<void()> cb = std::bind(&Connection::echo, this, sock->getFd());
    clnt_ch->setCallback(cb);
    clnt_ch->EnableReading();
}

Connection::~Connection()
{
    delete clnt_ch;
    clnt_ch = nullptr;
    delete sock;
    sock = nullptr;
}

void Connection::echo(int sockfd)
{
    char buf[READ_BUFFER];
    while (true)
    {
        int byte_read = read(sockfd, buf, READ_BUFFER);
        if (byte_read > 0)
        {
            cout << "Get messages from " << sockfd << ":" << buf << endl;
            write(sockfd, buf, sizeof(buf));
        }
        else if (byte_read == -1 && errno == EINTR)
        {
            // 客户端正常中断
            cout << "contineu reading..." << endl;
            continue;
        }
        else if (byte_read == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        {
            // 非阻塞IO表示数据全部读取完毕
            cout << "Finish reading once" << endl;
            break;
        }
        else if (byte_read == 0)
        {
            cout << "EOF, client " << sockfd << " disconnected!" << endl;
            deleteConnectionCallback(sock); // 回调函数关闭连接
            break;
        }
    }
}

void Connection::setDelteConnectionCallback(std::function<void(Socket *)> cb)
{
    deleteConnectionCallback = cb;
}
