#include "HttpServer.h"

int main() {
    HttpServer* server = new HttpServer();
    server->start();
    return 0;
}