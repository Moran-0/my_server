#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "Buffer.h"
#include "InetAddress.h"
#include "Socket.h"
#include "Util.h"

#define BUFFER_SIZE 1024
using std::cout;
using std::endl;

int main()
{
    Socket clnt_sock;
    InetAddress serv_addr("127.0.0.1", 8888);
    clnt_sock.connect(serv_addr);
    cout << "Connected to server!" << '\n';
    cout << endl;
    while (true)
    {
        Buffer sendBuffer;
        sendBuffer.Getline();
        size_t write_bytes = write(clnt_sock.getFd(), sendBuffer.c_str(), sendBuffer.size());
        if (write_bytes == -1)
        {
            printf("socket already disconnected, can't write any more!\n");
            break;
        }
    }
    return 0;
}
