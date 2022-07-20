#pragma once
#include <string>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <functional>
#include <memory>
#include <zmq.hpp>
#include "Serializer.hpp"

class Serializer;

template <typename T>
struct type_xx
{
	typedef T type;
};

template <>
struct type_xx<void>
{
	typedef int8_t type;
};

// 打包帮助模板
template <typename Tuple, std::size_t... Is>
void package_params_impl(Serializer &ds, const Tuple &t, std::index_sequence<Is...>)
{
	// 初始化列表展开（变长的模板参数）
	// 首先执行 'ds << std::get<Is>(t))'，完成参数的序列化
	(void)initializer_list<int>{((ds << std::get<Is>(t)), 0)...};
}

template <typename... Args>
void package_params(Serializer &ds, const std::tuple<Args...> &t)
{
	package_params_impl(ds, t, std::index_sequence_for<Args...>{});
}

// 1. decltype(auto) 用于对转发函数或封装的返回类型进行推导，它使我们无需显式的指定 decltype 的参数表达式
// 2. tuple 用于做参数调用函数模板类
template <typename Function, typename Tuple, std::size_t... Index>
decltype(auto) invoke_impl(Function &&func, Tuple &&t, std::index_sequence<Index...>) // invoke = 调用
{
	return func(std::get<Index>(std::forward<Tuple>(t))...); // 函数调用过程
}

template <typename Function, typename Tuple>
decltype(auto) invoke(Function &&func, Tuple &&t)
{
	constexpr auto size = std::tuple_size<typename std::decay<Tuple>::type>::value;
	return invoke_impl(std::forward<Function>(func), std::forward<Tuple>(t), std::make_index_sequence<size>{});
}

// 调用帮助类，主要用于返回是否void的情况（std::is_same 用于比较两个类型是否相同，R和void相同则返回0）
template <typename R, typename F, typename ArgsTuple>
typename std::enable_if<std::is_same<R, void>::value, typename type_xx<R>::type>::type
call_helper(F f, ArgsTuple args)
{
	invoke(f, args);
	return 0;
}

template <typename R, typename F, typename ArgsTuple>
typename std::enable_if<!std::is_same<R, void>::value, typename type_xx<R>::type>::type
call_helper(F f, ArgsTuple args)
{
	return invoke(f, args);
}

// rpc 类定义
class buttonrpc
{
public:
	enum rpc_role
	{
		RPC_CLIENT,
		RPC_SERVER
	};
	enum rpc_err_code
	{
		RPC_ERR_SUCCESS = 0,
		RPC_ERR_FUNCTIION_NOT_BIND,
		RPC_ERR_RECV_TIMEOUT
	};

	// wrap return value
	template <typename T>
	class value_t
	{
	public:
		typedef typename type_xx<T>::type type;
		typedef std::string msg_type;
		typedef uint16_t code_type;

		value_t()
		{
			code_ = 0;
			msg_.clear();
		}
		bool valid() { return (code_ == 0 ? true : false); }
		int error_code() { return code_; }
		std::string error_msg() { return msg_; }
		type val() { return val_; }

		void set_val(const type &val) { val_ = val; }
		void set_code(code_type code) { code_ = code; }
		void set_msg(msg_type msg) { msg_ = msg; }

		friend Serializer &operator>>(Serializer &in, value_t<T> &d)
		{
			in >> d.code_ >> d.msg_;
			if (d.code_ == 0)
			{
				in >> d.val_;
			}
			return in;
		}

		friend Serializer &operator<<(Serializer &out, value_t<T> d)
		{
			out << d.code_ << d.msg_ << d.val_;
			return out;
		}

	private:
		code_type code_;
		msg_type msg_;
		type val_;
	};

	buttonrpc();
	~buttonrpc();

	// network
	void as_client(std::string ip, int port);
	void as_server(int port);
	void send(zmq::message_t &data);
	void recv(zmq::message_t &data);
	void set_timeout(uint32_t ms);
	void run();

public:
	// server
	template <typename F>
	void bind(std::string name, F func);

	template <typename F, typename S>
	void bind(std::string name, F func, S *s);

	// client
	template <typename R, typename... Params> // 有参数
	value_t<R> call(std::string name, Params... ps);

	template <typename R> // 无参数
	value_t<R> call(std::string name);

private:
	Serializer *call_(std::string name, const char *data, int len);

	template <typename R>
	value_t<R> net_call(Serializer &ds);

	template <typename F>
	void callproxy(F fun, Serializer *pr, const char *data, int len);

	template <typename F, typename S>
	void callproxy(F fun, S *s, Serializer *pr, const char *data, int len);

	/*
	template<typename F>
	inline void buttonrpc::callproxy(F fun, Serializer* pr, const char* data, int len )
	{
		callproxy_(fun, pr, data, len);
	}
			↓ 转至
	*/
	// 函数指针
	template <typename R, typename... Params>
	void callproxy_(R (*func)(Params...), Serializer *pr, const char *data, int len)
	{
		callproxy_(std::function<R(Params...)>(func), pr, data, len);
	}

	// 类成员函数指针
	template <typename R, typename C, typename S, typename... Params>
	void callproxy_(R (C::*func)(Params...), S *s, Serializer *pr, const char *data, int len)
	{

		using args_type = std::tuple<typename std::decay<Params>::type...>;

		Serializer ds(StreamBuffer(data, len)); // 函数参数
		constexpr auto N = std::tuple_size<typename std::decay<args_type>::type>::value;
		args_type args = ds.get_tuple<args_type>(std::make_index_sequence<N>{});
		auto ff = [=](Params... ps) -> R { // lambda
			return (s->*func)(ps...);
		};
		typename type_xx<R>::type r = call_helper<R>(ff, args);

		value_t<R> val; //
		val.set_code(RPC_ERR_SUCCESS);
		val.set_val(r);
		(*pr) << val;
	}

	// functional
	template <typename R, typename... Params>
	void callproxy_(std::function<R(Params... ps)> func, Serializer *pr, const char *data, int len)
	{

		using args_type = std::tuple<typename std::decay<Params>::type...>;

		Serializer ds(StreamBuffer(data, len)); // 函数参数
		constexpr auto N = std::tuple_size<typename std::decay<args_type>::type>::value;
		args_type args = ds.get_tuple<args_type>(std::make_index_sequence<N>{});
		// call_helper 用来辅助判断调用函数是否需要返回值, 如果调用函数是void类型, 返回值默认为0
		typename type_xx<R>::type r = call_helper<R>(func, args);

		value_t<R> val;
		val.set_code(RPC_ERR_SUCCESS);
		val.set_val(r);
		(*pr) << val;
	}

private:
	std::map<std::string, std::function<void(Serializer *, const char *, int)>> m_handlers; // name -> function

	zmq::context_t m_context;
	std::unique_ptr<zmq::socket_t, std::function<void(zmq::socket_t *)>> m_socket;

	rpc_err_code m_error_code;

	int m_role;
};

inline buttonrpc::buttonrpc() : m_context(1)
{
	m_error_code = RPC_ERR_SUCCESS;
}

inline buttonrpc::~buttonrpc()
{
	m_context.close();
}

// network
inline void buttonrpc::as_client(std::string ip, int port)
{
	m_role = RPC_CLIENT;
	// 使用智能指针，销毁的时候调用sock->close()关闭连接
	m_socket = std::unique_ptr<zmq::socket_t, std::function<void(zmq::socket_t *)>>(new zmq::socket_t(m_context, ZMQ_REQ), [](zmq::socket_t *sock)
																					{ sock->close(); delete sock; sock =nullptr; });
	ostringstream os;
	os << "tcp://" << ip << ":" << port;
	m_socket->connect(os.str());
}

inline void buttonrpc::as_server(int port)
{
	m_role = RPC_SERVER;
	m_socket = std::unique_ptr<zmq::socket_t, std::function<void(zmq::socket_t *)>>(new zmq::socket_t(m_context, ZMQ_REP), [](zmq::socket_t *sock)
																					{ sock->close(); delete sock; sock =nullptr; });
	ostringstream os;
	os << "tcp://*:" << port;
	m_socket->bind(os.str()); // 绑定端口 --> ZMQ_EXPORT int zmq_bind (void *s, const char *addr);
}

inline void buttonrpc::send(zmq::message_t &data)
{
	m_socket->send(data);
}

inline void buttonrpc::recv(zmq::message_t &data)
{
	// m_socket->recv(&data, ZMQ_DONTWAIT);  nonblock mode
	m_socket->recv(&data);
}

inline void buttonrpc::set_timeout(uint32_t ms)
{
	// only client can set
	if (m_role == RPC_CLIENT)
	{
		m_socket->setsockopt(ZMQ_RCVTIMEO, ms);
	}
}

// 绑定好端口开始监听数据流，一次只允许获取一个客户端的请求
inline void buttonrpc::run()
{
	// only server can call
	if (m_role != RPC_SERVER)
	{
		return;
	}
	while (1)
	{
		zmq::message_t data; // zmq消息类型
		recv(data);			 // 通过固定端口获取客户端的数据，直接调用ZMQ API

		// std::cout << "[DEBUG] I got a message." << std::endl;

		StreamBuffer iodev((char *)data.data(), data.size()); // 实际上是个 vector<char>
		Serializer ds(iodev);

		std::string funname;
		ds >> funname;															  // 获取函数名
		Serializer *r = call_(funname, ds.current(), ds.size() - funname.size()); // 执行call_，调用相应函数

		zmq::message_t retmsg(r->size());
		memcpy(retmsg.data(), r->data(), r->size());
		send(retmsg); // 返回序列化结果
		delete r;
	}
}

// 处理函数相关
inline Serializer *buttonrpc::call_(std::string name, const char *data, int len)
{
	Serializer *ds = new Serializer();
	if (m_handlers.find(name) == m_handlers.end())
	{
		(*ds) << value_t<int>::code_type(RPC_ERR_FUNCTIION_NOT_BIND);
		(*ds) << value_t<int>::msg_type("function not bind: " + name);
		return ds;
	}
	auto fun = m_handlers[name]; // 函数类型：std::function<void(Serializer*, const char*, int)>
	fun(ds, data, len);			 // 执行函数，得到序列化结果(ds)
	ds->reset();				 // 偏移序列化对象的指针至开始位置
	return ds;
}

// func为绑定的函数，三个placeholder分别代表pr，data，len
template <typename F>
inline void buttonrpc::bind(std::string name, F func)
{
	m_handlers[name] = std::bind(&buttonrpc::callproxy<F>, this, func, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

// 如果为类成员函数则需要传入类对象s
template <typename F, typename S>
inline void buttonrpc::bind(std::string name, F func, S *s)
{
	m_handlers[name] = std::bind(&buttonrpc::callproxy<F, S>, this, func, s, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

template <typename F>
inline void buttonrpc::callproxy(F fun, Serializer *pr, const char *data, int len)
{
	callproxy_(fun, pr, data, len);
}

template <typename F, typename S>
inline void buttonrpc::callproxy(F fun, S *s, Serializer *pr, const char *data, int len)
{
	callproxy_(fun, s, pr, data, len);
}

template <typename R, typename... Params> // 有参数
inline buttonrpc::value_t<R> buttonrpc::call(std::string name, Params... ps)
{
	using args_type = std::tuple<typename std::decay<Params>::type...>;
	args_type args = std::make_tuple(ps...);
	Serializer ds;
	// 先序列化函数名称，再序列化函数参数
	ds << name;
	package_params(ds, args); // ds << args
	return net_call<R>(ds);
}

template <typename R> // 无参数
inline buttonrpc::value_t<R> buttonrpc::call(std::string name)
{
	Serializer ds;
	ds << name;				// 序列化函数名
	return net_call<R>(ds); // 远程调用过程
}

// 实际远程调用函数，上层函数为call()
template <typename R>
inline buttonrpc::value_t<R> buttonrpc::net_call(Serializer &ds)
{
	zmq::message_t request(ds.size() + 1);		  // 构造zmq的消息对象
	memcpy(request.data(), ds.data(), ds.size()); // copy数据到消息对象中
	if (m_error_code != RPC_ERR_RECV_TIMEOUT)
	{
		send(request); // 发送序列化请求
	}
	zmq::message_t reply;
	recv(reply); // 获取执行结果
	value_t<R> val;
	if (reply.size() == 0)
	{
		// timeout
		m_error_code = RPC_ERR_RECV_TIMEOUT;
		val.set_code(RPC_ERR_RECV_TIMEOUT);
		val.set_msg("recv timeout");
		return val;
	}
	m_error_code = RPC_ERR_SUCCESS;
	ds.clear();
	ds.write_raw_data((char *)reply.data(), reply.size());
	ds.reset();

	ds >> val;
	return val;
}