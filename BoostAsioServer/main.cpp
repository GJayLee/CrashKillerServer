//同步方式

//#include <iostream>
//#include <boost/asio.hpp>
//int main(int argc, char* argv[])
//{
//	using namespace boost::asio;
//	// 所有asio类都需要io_service对象
//	io_service iosev;
//	ip::tcp::acceptor acceptor(iosev, ip::tcp::endpoint(ip::tcp::v4(), 1000));
//	for (;;)
//	{
//		// socket对象
//		ip::tcp::socket socket(iosev);
//
//		// 等待直到客户端连接进来
//		acceptor.accept(socket);
//
//		// 显示连接进来的客户端
//		std::cout << socket.remote_endpoint().address() << std::endl;
//
//		// 向客户端发送hello world!
//		boost::system::error_code ec;
//		socket.write_some(buffer("hello world!"), ec);
//		// 如果出错，打印出错信息
//		if (ec)
//		{
//			std::cout << boost::system::system_error(ec).what() << std::endl;
//			break;
//		}
//		// 与当前客户交互完成后循环继续等待下一客户连接
//	}
//	return 0;
//}


//异步方式
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost\unordered_map.hpp>
#include <boost/smart_ptr.hpp>

#include "RWHandler.h"
#include "MyHttpHandler.h"

using namespace boost::asio;
using boost::system::error_code;
using ip::tcp;

//最大连接数
const int MaxConnectionNum = 65536;
const int MaxRecvSize = 65536;
//设置服务器更新时间为5分钟
const int UPDATE_TIME = 300;

//从萌友平台获取异常数据时的默认appKey、start_date和end_date
string defaultAppKey = "9e4b010a0d51f6e020ead6ce37bad33896a00f90";
string defaultStartDate = "2016-07-20";
string defaultEndDate = "2016-07-26";

struct CHelloWorld_Service
{
	CHelloWorld_Service(io_service &iosev):m_iosev(iosev)
		, m_acceptor(iosev, tcp::endpoint(tcp::v4(),80)), m_timer(iosev, boost::posix_time::seconds(UPDATE_TIME))
		, m_cnnIdPool(MaxConnectionNum)
		
	{
		
		/*httphandler = new MyHttpHandler("9e4b010a0d51f6e020ead6ce37bad33896a00f90", "2016-07-20", "2016-07-26");
		httphandler->PostHttpRequest();
		httphandler->ParseJsonAndInsertToDatabase();*/
		
		httphandler = new MyHttpHandler();
		//此处的http请求应改为每隔一段时间触发
		m_timer.async_wait(boost::bind(&CHelloWorld_Service::wait_handler, this));

		int current = 0;
		std::generate_n(m_cnnIdPool.begin(), MaxConnectionNum, [&current] {return ++current; });
	}
	//监听连接
	void Accept()
	{
		//CheckHandlers();
		std::cout << "Start Listening " << std::endl;
		std::shared_ptr<RWHandler> handler = CreateHandler();
		m_acceptor.async_accept(handler->GetSocket(), [this, handler](const boost::system::error_code& error)
		{
			if (error)
			{
				std::cout << error.value() << " " << error.message() << std::endl;
				HandleAcpError(handler, error);
			}

			m_handlers.insert(std::make_pair(handler->GetConnId(), handler));
			std::cout << "current connect count: " << m_handlers.size() << std::endl;

			//此处的http请求应改为每隔一段时间触发
			/*MyHttpHandler *httphandler = new MyHttpHandler("9e4b010a0d51f6e020ead6ce37bad33896a00f90", "2016-05-20", "2016-07-26");
			string errorList = httphandler->PostHttpRequest();
			httphandler->ParseJsonAndInsertToDatabase();*/

			handler->HandleRead();
			appKey = handler->GetAppKey();
			start_date = handler->GetStartDate();
			end_date = handler->GetEndDate();

			//handler->setSendData(errorList.c_str());
			//handler->HandleWrite();

			deadline_timer t(m_iosev, boost::posix_time::seconds(3));
			t.wait();

			Accept();
		});
	}
	
	//开始等待连接
	void start()
	{
		// 开始等待连接（非阻塞）
		boost::shared_ptr<tcp::socket> psocket(new tcp::socket(m_iosev));
		// 触发的事件只有error_code参数，所以用boost::bind把socket绑定进去
		m_acceptor.async_accept(*psocket,
		boost::bind(&CHelloWorld_Service::accept_handler, this, psocket,_1));
	}

	// 有客户端连接时accept_handler触发
	void accept_handler(boost::shared_ptr<tcp::socket> psocket, error_code ec)
	{
		if (ec)
			return;
		// 继续等待连接
		
		//接受消息（非阻塞）
		boost::shared_ptr<std::vector<char> > str(new std::vector<char>(100, 0));
		psocket->async_read_some(buffer(*str), bind(&CHelloWorld_Service::read_handler, this, placeholders::error, str));

		// 显示远程IP
		std::cout << psocket->remote_endpoint().address() << std::endl;
		// 发送信息(非阻塞)
		boost::shared_ptr<std::string> pstr(new
			std::string("hello async world!"));
		psocket->async_write_some(buffer(*pstr),
			boost::bind(&CHelloWorld_Service::write_handler, this, pstr, _1,_2));

		/*boost::shared_ptr<std::string> pReadstr;
		psocket->async_read_some(buffer(*pReadstr),
			boost::bind(&CHelloWorld_Service::read_handler, this, _1, _2));
		std::cout << *pstr << std::endl;*/

		deadline_timer t(m_iosev, boost::posix_time::seconds(2));
		t.wait();
		start();
	}

	// 异步写操作完成后write_handler触发
	void write_handler(boost::shared_ptr<std::string> pstr, error_code ec,size_t bytes_transferred)
	{
		if (ec)
			std::cout << "发送失败!" <<
			std::endl;
		else
			std::cout << *pstr << " 已发送" <<
			std::endl;
	}

	//异步读操作完成后read_handler触发
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str)
	{
		if (ec)
		{
			std::cout << "没有接收到消息！" << std::endl;
		}
		else
		{
			std::cout << "接收消息：" << &(*str)[0] << std::endl;
		}
	}
	/*void read_handler(boost::shared_ptr<tcp::socket> psocket, error_code ec, size_t bytes_transferred)
	{
		if (ec)
			std::cout << "没有接收到数据！" << std::endl;
		else
		{
			boost::shared_ptr<std::string> pstr;
			psocket->async_read_some(buffer(*pstr),
				boost::bind(&CHelloWorld_Service::read_handler, this, _1, _2));
			std::cout << *pstr << std::endl;
		}
	}*/

private:
	//每隔5分钟调用一次http请求更新数据
	void wait_handler()
	{
		//如果appkey、start_date和end_date为空，则赋予它们一个默认值进行更新
		if (appKey == "")
			appKey = defaultAppKey;
		if (start_date == "")
			start_date = defaultStartDate;
		if (end_date == "")
			end_date = defaultEndDate;
		//此处的http请求应改为每隔一段时间触发
		httphandler->setAppKey(appKey);
		httphandler->setStartDate(start_date);
		httphandler->setEndDate(end_date);
		httphandler->PostHttpRequest();
		httphandler->ParseJsonAndInsertToDatabase();

		m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(UPDATE_TIME));
		m_timer.async_wait(boost::bind(&CHelloWorld_Service::wait_handler, this));
	}

	//检测哪些socket断开
	void CheckHandlers()
	{
		if (m_handlers.size() <= 0)
			return;
		boost::unordered_map<int, std::shared_ptr<RWHandler>>::iterator iter = m_handlers.begin();
		for (; iter != m_handlers.end();iter++)
		{
			iter->second->HandleRead();
		}
	}
	//发生连接错误时调用
	void HandleAcpError(std::shared_ptr <RWHandler> eventHanlder, const boost::system::error_code& error)
	{
		std::cout << "Error，error reason：" << error.value() << error.message() << std::endl;
		//关闭socket，移除读事件处理器
		eventHanlder->CloseSocket();
		StopAccept();
	}

	void StopAccept()
	{
		boost::system::error_code ec;
		m_acceptor.cancel(ec);
		m_acceptor.close(ec);
		m_iosev.stop();
	}

	std::shared_ptr<RWHandler> CreateHandler()
	{
		int connId = m_cnnIdPool.front();
		m_cnnIdPool.pop_front();
		std::shared_ptr<RWHandler> handler = std::make_shared<RWHandler>(m_iosev);

		handler->SetConnId(connId);

		handler->SetCallBackError([this](int connId)
		{
			RecyclConnid(connId);
		});

		return handler;
	}
	//循环使用socket ID
	void RecyclConnid(int connId)
	{
		auto it = m_handlers.find(connId);
		if (it != m_handlers.end())
			m_handlers.erase(it);
		//std::cout << "current connect count: " << m_handlers.size() << std::endl;
		m_cnnIdPool.push_back(connId);
	}


	io_service &m_iosev;
	ip::tcp::acceptor m_acceptor;

	boost::unordered_map<int, std::shared_ptr<RWHandler>> m_handlers;
	std::list<int> m_cnnIdPool;

	deadline_timer m_timer;
	string errorList;
	MyHttpHandler *httphandler;

	string appKey;
	string start_date;
	string end_date;
};



int main()
{
	io_service iosev;

	CHelloWorld_Service sev(iosev);

	// 开始等待连接
	//sev.start();
	sev.Accept();

	iosev.run();

	return 0;
}


//#include <boost/asio/buffer.hpp>
//#include <boost/unordered_map.hpp>
//#include "Message.h"
//#include "RWHandler.h"
//
//const int MaxConnectionNum = 65536;
//const int MaxRecvSize = 65536;
//class Server
//{
//public:
//
//	Server(io_service& ios, short port) : m_ios(ios), m_acceptor(ios, ip::tcp::endpoint(ip::tcp::v4(), port)), m_cnnIdPool(MaxConnectionNum)
//	{
//		int current = 0;
//		std::generate_n(m_cnnIdPool.begin(), MaxConnectionNum, [&current] {return ++current; });
//	}
//
//	~Server()
//	{
//	}
//
//	void Accept()
//	{
//		std::cout << "Start Listening " << std::endl;
//		std::shared_ptr<RWHandler> handler = CreateHandler();
//
//		m_acceptor.async_accept(handler->GetSocket(), [this, handler](const boost::system::error_code& error)
//		{
//			if (error)
//			{
//				std::cout << error.value() << " " << error.message() << std::endl;
//				HandleAcpError(handler, error);
//			}
//
//			m_handlers.insert(std::make_pair(handler->GetConnId(), handler));
//			std::cout << "current connect count: " << m_handlers.size() << std::endl;
//
//			handler->HandleRead();
//			Accept();
//		});
//	}
//
//private:
//	void HandleAcpError(std::shared_ptr <RWHandler> eventHanlder, const boost::system::error_code& error)
//	{
//		std::cout << "Error，error reason：" << error.value() << error.message() << std::endl;
//		//关闭socket，移除读事件处理器
//		eventHanlder->CloseSocket();
//		StopAccept();
//	}
//
//	void StopAccept()
//	{
//		boost::system::error_code ec;
//		m_acceptor.cancel(ec);
//		m_acceptor.close(ec);
//		m_ios.stop();
//	}
//
//	std::shared_ptr<RWHandler> CreateHandler()
//	{
//		int connId = m_cnnIdPool.front();
//		m_cnnIdPool.pop_front();
//		std::shared_ptr<RWHandler> handler = std::make_shared<RWHandler>(m_ios);
//
//		handler->SetConnId(connId);
//
//		handler->SetCallBackError([this](int connId)
//		{
//			RecyclConnid(connId);
//		});
//
//		return handler;
//	}
//
//	void RecyclConnid(int connId)
//	{
//		auto it = m_handlers.find(connId);
//		if (it != m_handlers.end())
//			m_handlers.erase(it);
//		std::cout << "current connect count: " << m_handlers.size() << std::endl;
//		m_cnnIdPool.push_back(connId);
//	}
//
//private:
//	io_service& m_ios;
//	ip::tcp::acceptor m_acceptor;    
//	boost::unordered_map<int, std::shared_ptr<RWHandler>> m_handlers;
//
//	std::list<int> m_cnnIdPool;
//};
//
//int main()
//{
//	io_service ios;
//	Server server(ios, 1000);
//	server.Accept();
//	ios.run();
//
//	return 0;
//}