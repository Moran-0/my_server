#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "src/Util.h"
#include "src/Socket.h"
#include "src/InetAddress.h"
#include "src/Buffer.h"

#define BUFFER_SIZE 1024
using std::cout;
using std::endl;

int main()
{
    Socket clnt_sock;
    InetAddress serv_addr("127.0.0.1", 8888);
    clnt_sock.connect(serv_addr);

    while (true)
    {
        Buffer sendBuffer, readBuffer;
        sendBuffer.getline();
        size_t write_bytes = write(clnt_sock.getFd(), sendBuffer.c_str(), sendBuffer.size());
        if (write_bytes == -1)
        {
            printf("socket already disconnected, can't write any more!\n");
            break;
        }
        // sendBuffer.clear();
        char buf[BUFFER_SIZE];
        while (true)
        {
            size_t read_bytes = read(clnt_sock.getFd(), buf, sizeof(buf));
            if (read_bytes > 0)
            {
                readBuffer.append(buf, read_bytes);
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
            if (readBuffer.size() >= sendBuffer.size())
            {
                cout << "message from server: " << readBuffer.c_str() << endl;
                break;
            }
        }
        readBuffer.clear();
    }
    return 0;
}