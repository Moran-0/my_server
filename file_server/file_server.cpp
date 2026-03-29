#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <string>

#define BUF_SIZE 1024

void error_handling(const char *message);

int main(int argc, char *argv[])
{
    try
    {
        int serv_sk, clnt_sk;
        FILE *fp;
        char buf[BUF_SIZE];
        sockaddr_in serv_addr, clnt_addr;
        socklen_t clnt_len;
        if (argc != 2)
        {
            std::cerr << "Usage:" << argv[0] << "<port>" << std::endl;
            exit(1);
        }
        memset(&serv_addr, 0, sizeof(serv_addr)); // 初始化前置0
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 接受任意客户端的连接
        serv_addr.sin_port = htons(std::stoi(argv[1]));
        serv_sk = socket(AF_INET, SOCK_STREAM, 0);
        if (serv_sk == -1)
        {
            error_handling("socket create failed!");
        }
        if (bind(serv_sk, (sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        {
            error_handling("bind socket failed!");
        }
        // 监听
        listen(serv_sk, 10);
        clnt_len = sizeof(clnt_addr);
        // 建立连接
        clnt_sk = accept(serv_sk, (sockaddr *)&clnt_addr, &clnt_len);
        if (clnt_sk == -1)
        {
            error_handling("accept error");
        }
        std::cout << "connect:" << clnt_sk << std::endl;
        fp = fopen("file.txt", "rb");
        // 数据传输
        while (true)
        {
            int read_file = fread((void *)buf, 1, BUF_SIZE, fp);
            std::cout << read_file << std::endl;
            if (read_file < BUF_SIZE)
            {
                write(clnt_sk, (void *)buf, read_file);
                break;
            }
            std::cout << std::string(buf) << std::endl;
            write(clnt_sk, (void *)buf, BUF_SIZE);
        }
        std::cout << "test1" << std::endl;
        // 半关闭输出流
        shutdown(clnt_sk, SHUT_WR);
        std::cout << "test2" << std::endl;
        read(clnt_sk, (void *)buf, BUF_SIZE);
        std::cout << "test3" << std::endl;
        std::cout << "Message from client" << clnt_sk << buf << std::endl;
        // 关闭文件和套接字
        fclose(fp);
        close(clnt_sk);
        close(serv_sk);
        return 0;
    }
    catch (const std::exception &e)
    {
        error_handling(e.what());
    }
}

void error_handling(const char *message)
{
    std::cout << message << std::endl;
    exit(1);
}