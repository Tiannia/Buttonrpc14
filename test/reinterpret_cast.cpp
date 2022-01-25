#include <iostream>
using namespace std;
int main()
{
    uint16_t a = 2;
    cout << "&a = " << &a << endl;
    char *p1 = reinterpret_cast<char *>(&a);
    char *p2 = reinterpret_cast<char *>(a);
    cout << *(int *)p1 << endl;
    cout << int(*p1) << endl;
    cout << "*p1:" << *p1 << '?' << endl;
    int b = *reinterpret_cast<uint16_t *>(p1);
    cout << b << endl;
    //cout << p2 << endl;
    return 0;
}