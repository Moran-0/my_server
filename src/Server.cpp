#include "Server.h"
#include "Socket.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Channel.h"
#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define READ_BUFFER 1024
using std::cout;
using std::endl;
Server::Server(EventLoop *_loop) : loop(_loop)
{
    Socket *serv_sock = new Socket();
    InetAddress serv_addr("127.0.0.1", 8888);
    serv_sock->bind(serv_addr);
    serv_sock->setNonBlocking();
    serv_sock->listen();

    Channel *serv_channel = new Channel(loop, serv_sock->getFd()); // 为服务器创建监听channel
    std::function<void()> cb = std::bind(&Server::newConnection, this, serv_sock);
    serv_channel->setCallback(cb);
    serv_channel->EnableReading(); // 注册并监听可读事件
    cout << "Server " << serv_sock->getFd() << " Start!" << endl;
}

Server::~Server()
{
}

void Server::handReadEvent(int sockfd)
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
            close(sockfd);
            break;
        }
    }
}

void Server::newConnection(Socket *serv_sk)
{
    InetAddress *clnt_addr = new InetAddress();
    Socket *clnt_sock = new Socket(serv_sk->accept(clnt_addr));
    cout << "Connect to client " << clnt_sock->getFd() << "IP:" << inet_ntoa(clnt_addr->addr.sin_addr) << " PORT:" << clnt_addr->addr.sin_port << endl;
    std::function<void()> cb = std::bind(&Server::handReadEvent, this, clnt_sock->getFd());
    clnt_sock->setNonBlocking();
    Channel *clnt_ch = new Channel(loop, clnt_sock->getFd());
    clnt_ch->setCallback(cb);
    clnt_ch->EnableReading();
}
