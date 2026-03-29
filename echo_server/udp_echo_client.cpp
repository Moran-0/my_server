#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>

#define BUF_SIZE 1024
void error_handling(const char *message);
int main(int argc, char *argv[])
{
    int sock;
    char message[BUF_SIZE];
    int str_len;

    sockaddr_in serv_adr, from_adr;
    socklen_t from_adr_sz;
    if (argc != 3)
    {
        printf("Usage:%s <addr> <port>", argv[0]);
        exit(1);
    }
    serv_adr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &serv_adr.sin_addr.s_addr);
    serv_adr.sin_port = htons(atoi(argv[2]));
    sock = socket(AF_INET, SOCK_DGRAM, 0); // ipv4, udp
    if (sock == -1)
        error_handling("socket() error");
    // if (bind(sock, (sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
    //     error_handling("bind() error");
    // 无需绑定，udp共用一个sock
    std::cout << "Scoket:" << sock << std::endl;
    while (true)
    {
        fputs("Input message(Q to quit): ", stdout);
        fgets(message, BUF_SIZE, stdin);
        if (!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
            break;
        sendto(sock, message, strlen(message), 0, (sockaddr *)&serv_adr, sizeof(serv_adr));
        from_adr_sz = sizeof(from_adr);
        str_len = recvfrom(sock, message, BUF_SIZE, 0, (sockaddr *)&from_adr, &from_adr_sz);
        message[str_len] = '\0';
        printf("Receive:%s", message);
    }
    close(sock);
    return 0;
}
void error_handling(const char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}