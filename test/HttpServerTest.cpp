#include "HttpServer.h"
#include "AsyncLogging.h"
#include "Logging.h"

std::unique_ptr<AsyncLogging> g_asyncLog;
void AsyncOutput(const char* msg, int len) {
    g_asyncLog->Append(msg, len);
}
void AysncFlushFunc() {
    g_asyncLog->RequestFlush();
}
int main() {
    g_asyncLog = std::make_unique<AsyncLogging>("./mylog/TestAsync.log");
    Logger::setOutput(AsyncOutput);
    Logger::setFlush(AysncFlushFunc);
    g_asyncLog->Start();

    HttpServer* server = new HttpServer("127.0.0.1", 8888);
    server->start();
    return 0;
}