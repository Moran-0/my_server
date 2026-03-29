#pragma once
class EventLoop;
class Socket;
class Acceptor;
class Server
{
private:
    EventLoop *loop;
    Acceptor *acceptor;

public:
    Server(EventLoop *_loop);
    ~Server();
    void handReadEvent(int);
    void newConnection(Socket *serv_sk);
};