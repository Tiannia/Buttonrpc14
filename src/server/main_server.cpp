#include <string>
#include <iostream>
#include "buttonrpc.hpp"

#define buttont_assert(exp)                                                                   \
	{                                                                                         \
		if (!(exp))                                                                           \
		{                                                                                     \
			std::cout << "ERROR: ";                                                           \
			std::cout << "function: " << __FUNCTION__ << ", line: " << __LINE__ << std::endl; \
		}                                                                                     \
	}

// 测试例子
void foo_1() {
	
}

void foo_2(int arg1) {
	buttont_assert(arg1 == 10);
}

int foo_3(int arg1) {
	buttont_assert(arg1 == 10);
	return arg1 * arg1;
}

int foo_4(int arg1, std::string arg2, int arg3, float arg4) {
	buttont_assert(arg1 == 10);
	buttont_assert(arg2 == "buttonrpc");
	buttont_assert(arg3 == 100);
	buttont_assert((arg4 > 10.0) && (arg4 < 11.0));
	return arg1 * arg3;
}

class ClassMem
{
public:
	int bar(int arg1, std::string arg2, int arg3) {
		buttont_assert(arg1 == 10);
		buttont_assert(arg2 == "buttonrpc");
		buttont_assert(arg3 == 100);
		return arg1 * arg3;
	}
};

struct PersonInfo
{
	int age;
	std::string name;
	float height;

	// must implement
	friend Serializer& operator >> (Serializer& in, PersonInfo& d) {
		in >> d.age >> d.name >> d.height;
		return in;
	}
	friend Serializer& operator << (Serializer& out, PersonInfo d) {
		out << d.age << d.name << d.height;
		return out;
	}
};

PersonInfo foo_5(PersonInfo d,  int weigth)
{
	buttont_assert(d.age == 10);
	buttont_assert(d.name == "buttonrpc");
	buttont_assert(d.height == 170);

	PersonInfo ret;
	ret.age = d.age + 10;
	ret.name = d.name + " is good";
	ret.height = d.height + 10;
	return ret;
}

int main()
{
	buttonrpc server;
	// 1. 完成被调用函数的注册，
	// 2. 在收到网络发送的数据进行序列化还原后找到对应的调用函数和参数，
	// 3. 执行调用再将结果返回。
	server.as_server(8888);

	server.bind("foo_1", foo_1);
	server.bind("foo_2", foo_2);
	server.bind("foo_3", std::function<int(int)>(foo_3));
	server.bind("foo_4", foo_4);
	server.bind("foo_5", foo_5);

	ClassMem s;
	server.bind("foo_6", &ClassMem::bar, &s);

	std::cout << "run rpc server on: " << 8888 << std::endl;
	server.run();

	return 0;
}
