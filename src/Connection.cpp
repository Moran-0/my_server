#include "Connection.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Buffer.h"

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

using std::cout;
using std::endl;

#define READ_BUFFER 512

Connection::Connection(EventLoop* _loop, int sock_fd) : loop(_loop)
{
    sock = std::make_unique<Socket>(sock_fd);
    sock->setNonBlocking();
    clnt_ch = std::make_unique<Channel>(loop, sock->getFd());
    readBuffer = new Buffer();
}

Connection::~Connection()
{
    delete readBuffer;
    readBuffer = nullptr;
}

void Connection::registerChannel()
{
    int sockfd = sock->getFd();
    clnt_ch->setReadCallback([this, sockfd]()
                             {
                                 if (echo(sockfd))
                                 {
                                     deleteConnectionCallback(sockfd);
                                 } });
    clnt_ch->enableRead();
}

bool Connection::echo(int sockfd)
{
    char buf[READ_BUFFER];
    while (true)
    {
        // std::fill(buf, buf + READ_BUFFER, 0);
        memset(buf, 0, READ_BUFFER); // 性能更优
        int byte_read = read(sockfd, buf, READ_BUFFER);
        if (byte_read > 0)
        {
            cout << "Get messages from " << sockfd << ":" << buf << endl;
            readBuffer->append(buf, byte_read);
            // write(sockfd, buf, sizeof(buf));
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
            if (!send(sockfd))
            {
                clnt_ch->disableAll();
                loop->removeChannel(clnt_ch.get());
                return true;
            }
            readBuffer->clear();
            break;
        }
        else if (byte_read == 0)
        {
            cout << "EOF, client " << sockfd << " disconnected!" << '\n';
            clnt_ch->disableAll();
            loop->removeChannel(clnt_ch.get());
            return true;
        }
    }
    return false;
}

void Connection::setDeleteConnectionCallback(std::function<void(int)> cb)
{
    deleteConnectionCallback = std::move(cb);
}

bool Connection::send(int sockfd)
{
    const char *buf = readBuffer->c_str();
    int data_size = static_cast<int>(readBuffer->size());
    int data_left = data_size;
    while (data_left > 0)
    {
        ssize_t bytes_write = ::send(sockfd, buf + data_size - data_left, data_left, MSG_NOSIGNAL);
        if (bytes_write > 0)
        {
            data_left -= static_cast<int>(bytes_write);
            continue;
        }
        if (bytes_write == -1 && errno == EINTR)
        {
            continue;
        }
        if (bytes_write == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }
        return false;
    }
    return true;
}
