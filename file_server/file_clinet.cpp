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
    FILE *fp;
    int sk;
    char *buf[BUF_SIZE];
    sockaddr_in serv_addr;
    socklen_t serv_len;
    if (argc != 3)
    {
        error_handling((std::string("") + "Usage" + argv[0] + "<addr> <port>").data());
    }

    // 创建套接字
    sk = socket(AF_INET, SOCK_STREAM, 0);
    if (sk == -1)
    {
        error_handling("create socket failed！");
    }
    // 设置地址和端口
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
    serv_addr.sin_port = htons(std::stoi(argv[2]));
    serv_len = sizeof(serv_addr);
    if (connect(sk, (sockaddr *)&serv_addr, serv_len) == -1)
    {
        error_handling("connect error!");
    }

    int read_cnt;
    fp = fopen("receive.txt", "wb");
    std::cout << "test1" << std::endl;
    while ((read_cnt = read(sk, buf, BUF_SIZE)) != 0)
    {
        std::cout << buf << std::endl;
        fwrite((void *)buf, 1, read_cnt, fp);
    }
    std::cout << "Receive data from server!" << std::endl;
    write(sk, "Thank You", 10);
    fclose(fp);
    close(sk);
    return 0;
}
void error_handling(const char *message)
{
    std::cout << message << std::endl;
    exit(1);
}