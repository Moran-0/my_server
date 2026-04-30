#include "LogStream.h"
#include <iostream>
int main() {

    LogStream os;
    os << "hello world";
    std::cout << os.Buffer().Data() << std::endl;
    os.ResetBuffer();

    os << 11;
    std::cout << os.Buffer().Data() << std::endl;
    os.ResetBuffer();

    os << 0.1;
    std::cout << os.Buffer().Data() << std::endl;
    os.ResetBuffer();

    return 0;
}