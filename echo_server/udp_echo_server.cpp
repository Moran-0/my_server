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
    int serv_sock;
    char message[BUF_SIZE];
    int str_len;
    sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;

    if (argc != 2)
    {
        printf("Usage : %s <addr> <port>\n", argv[0]);
        exit(1);
    }
    serv_sock = socket(AF_INET, SOCK_DGRAM, 0); // ipv4, udp
    if (serv_sock == -1)
        error_handling("socket() error");
    std::cout << "Scoket" << serv_sock << std::endl;
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));
    printf("test");

    if (bind(serv_sock, (sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    while (true)
    {
        clnt_adr_sz = sizeof(clnt_adr);
        str_len = recvfrom(serv_sock, message, BUF_SIZE, 0, (sockaddr *)&clnt_adr, &clnt_adr_sz);
        if (str_len == -1)
            error_handling("recvfrom() error");
        sendto(serv_sock, message, str_len, 0, (sockaddr *)&clnt_adr, clnt_adr_sz);
    }
    close(serv_sock);
    return 0;
}
void error_handling(const char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}