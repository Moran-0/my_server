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
    std::string message;
    while (true)
    {
        message.clear();
        std::getline(std::cin, message);
        Buffer sendBuffer, readBuffer;
        sendBuffer.Append(message);
        size_t write_bytes = write(clnt_sock.getFd(), sendBuffer.Peek(), sendBuffer.ReadableBytes());
        if (write_bytes == -1)
        {
            printf("socket already disconnected, can't write any more!\n");
            break;
        }
        // sendBuffer.Clear();

        char buf[BUFFER_SIZE];
        while (true)
        {
            size_t read_bytes = read(clnt_sock.getFd(), buf, sizeof(buf));
            if (read_bytes > 0)
            {
                readBuffer.Append(buf, read_bytes);
            }
            else if (read_bytes == 0)
            {
                printf("server socket disconnected!\n");
                exit(EXIT_SUCCESS);
            }
            else if (read_bytes == -1)
            {
                errif(true, "socket read error");
            }
            if (readBuffer.ReadableBytes() >= sendBuffer.ReadableBytes()) {
                cout << "message from server: " << readBuffer.Peek() << endl;
                break;
            }
        }
        readBuffer.RetrieveAll();
    }
    return 0;
}
