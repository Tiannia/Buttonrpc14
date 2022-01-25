#include <memory>
#include <string.h>
#include <iostream>

int main()
{
    const char* p = "123456";
    std::shared_ptr<char> d(new char[10]);
    memcpy(&(*d), p, 5);
    // char dd[5];
    // memcpy(dd, p, 4);
    std::cout << strlen(&(*d)) << std::endl;
    std::cout << d << std::endl;
    return 0;
}