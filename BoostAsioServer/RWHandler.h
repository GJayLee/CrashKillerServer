#pragma once
#include<iostream>
#include<boost\asio.hpp>
#include <boost\serialization\split_member.hpp>
#include<boost\tokenizer.hpp>
#include<boost\algorithm\string.hpp>

#include "Message.h"

#include "MyHttpHandler.h"

using namespace boost::asio;
using boost::asio::ip::tcp;

const int MAX_IP_PACK_SIZE = 100;
const int HEAD_LEN = 1;

const int SEND_SIZE = 100;

class RWHandler
{
public:

	RWHandler(io_service& ios) : m_sock(ios), m_timer(ios, boost::posix_time::seconds(300))
	{
		//此处的http请求应改为每隔一段时间触发
		/*std::cout << "Init" << std::endl;
		httphandler = new MyHttpHandler("9e4b010a0d51f6e020ead6ce37bad33896a00f90", "2016-07-20", "2016-07-26");
		string errorList = httphandler->PostHttpRequest();
		setSendData(errorList.c_str());
		httphandler->ParseJsonAndInsertToDatabase();*/

		//m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));

		offSet = 0;
		requestUpdate = false;
		initErrorInfo = false;
	}

	~RWHandler()
	{
	}

	void HandleRead()
	{
		//三种情况下会返回：1.缓冲区满；2.transfer_at_least为真(收到特定数量字节即返回)；3.有错误发生
		//async_read(m_sock, buffer(m_buff), transfer_at_least(HEAD_LEN), [this](const boost::system::error_code& ec, size_t size)
		//{
		//	if (ec != nullptr)
		//	{
		//		HandleError(ec);
		//		return;
		//	}
		//	
		//	//std::cout << m_buff.size() << "," << m_buff.data() << std::endl;

		//	//HandleRead();
		//	offSet = offSet + 10;
		//	HandleWrite();
		//});

		try 
		{
			//接受消息（非阻塞）
			boost::shared_ptr<std::vector<char> > str(new std::vector<char>(100, 0));
			m_sock.async_read_some(buffer(*str), bind(&RWHandler::read_handler, this, placeholders::error, str));
		}
		catch(std::exception& e)
		{
			std::cout << "读取错误" << std::endl;
		}
	}

	//void HandleWrite(char* data, int len)
	void HandleWrite()
	{
		/*boost::system::error_code ec;
		write(m_sock, buffer(data, len), ec);
		if (ec != nullptr)
			HandleError(ec);*/
		// 发送信息(非阻塞)
		/*boost::shared_ptr<std::string> pstr(new
			std::string(errorList));
		m_sock.async_write_some(buffer(str, len),
			boost::bind(&RWHandler::write_handler, this, placeholders::error),pstr);*/

		if (initErrorInfo)
		{
			int sendSize = 0;
			if (strlen(sendData) - offSet < SEND_SIZE)
				sendSize = strlen(sendData) - offSet;
			else
				sendSize = SEND_SIZE;
			m_sock.async_write_some(buffer(sendData + offSet, sendSize),
				boost::bind(&RWHandler::write_handler, this, placeholders::error));
		}
		
	}

	ip::tcp::socket& GetSocket()
	{
		return m_sock;
	}

	void CloseSocket()
	{
		boost::system::error_code ec;
		m_sock.shutdown(ip::tcp::socket::shutdown_send, ec);
		m_sock.close(ec);
	}

	void SetConnId(int connId)
	{
		m_connId = connId;
	}

	int GetConnId() const
	{
		return m_connId;
	}

	void setSendData(const char* str)
	{
		sendData = new char[strlen(str) + 1];
		memset(sendData, 0, sizeof(char)*(strlen(str) + 1));
		strcpy(sendData, str);
		offSet = 0;
	}

	template<typename F>
	void SetCallBackError(F f)
	{
		m_callbackError = f;
	}

private:
	//void do_read_header()
	//{
	//	//auto self(boost::shared_from_this());
	//	boost::asio::async_read(m_sock,
	//		boost::asio::buffer(read_msg.data(), Message::header_length),
	//		[this](boost::system::error_code ec, std::size_t /*length*/)
	//	{
	//		if (!ec && read_msg.decode_header())
	//		{
	//			do_read_body();
	//		}
	//		else
	//		{
	//			HandleError(ec);
	//			return;
	//		}
	//	});
	//}

	//void do_read_body()
	//{
	//	//auto self(shared_from_this());
	//	boost::asio::async_read(m_sock,
	//		boost::asio::buffer(read_msg.body(), read_msg.body_length()),
	//		[this](boost::system::error_code ec, std::size_t /*length*/)
	//	{
	//		if (!ec)
	//		{
	//			std::cout << "data" << read_msg.data() << std::endl;
	//			//room_.deliver(read_msg_);
	//			do_read_header();
	//		}
	//		else
	//		{
	//			HandleError(ec);
	//			return;
	//		}
	//	});
	//}

	//每隔5分钟调用一次http请求更新数据
	void wait_handler()
	{
		//此处的http请求应改为每隔一段时间触发
		httphandler->setAppKey("9e4b010a0d51f6e020ead6ce37bad33896a00f90");
		httphandler->setStartDate("2016-07-20");
		httphandler->setEndDate("2016-07-26");
		string errorList = httphandler->PostHttpRequest();
		setSendData(errorList.c_str());
		httphandler->ParseJsonAndInsertToDatabase();
	}

	void GetDatabaseData()
	{
		sql::Driver *dirver;
		sql::Connection *con;
		sql::Statement *stmt;
		sql::ResultSet *res;
		dirver = get_driver_instance();
		//连接数据库
		con = dirver->connect("localhost", "root", "123456");
		//选择mydata数据库
		con->setSchema("CrashKiller");
		//con->setClientOption("characterSetResults", "utf8");
		stmt = con->createStatement();

		string result = "";

		//从name_table表中获取所有信息
		res = stmt->executeQuery("select * from errorinfo");
		//循环遍历
		while (res->next())
		{
			//输出，id，name，age,work,others字段的信息
			//cout << res->getString("name") << " | " << res->getInt("age") << endl;
			result = result + res->getString("ID") + res->getString("context_digest") 
				+ res->getString("raw_crash_record_id") + res->getString("created_at")
				+ res->getString("app_version");
		}

		setSendData(result.c_str());

		//清理
		delete res;
		delete stmt;
		delete con;
	}

	// 异步写操作完成后write_handler触发
	//void write_handler(const boost::system::error_code& ec, boost::shared_ptr<std::string> str)
	void write_handler(const boost::system::error_code& ec)
	{
		if (ec)
		{
			std::cout << "发送失败!" << std::endl;
			HandleError(ec);
		}
		else
		{
			std::cout << "成功发送！" << std::endl;
			//接受消息（非阻塞）
			HandleRead();
		}
			
	}
	//异步读操作完成后read_handler触发
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str)
	{
		if (ec)
		{
			//std::cout << "没有接收到消息！" << std::endl;
			HandleError(ec);
			return;
		}
		else
		{
			if (initErrorInfo || strcmp(&(*str)[0], "Init") == 0)
			{
				if (!initErrorInfo)
				{
					initErrorInfo = true;
					GetDatabaseData();
					HandleWrite();
				}
				else
				{
					if ((offSet + SEND_SIZE) > strlen(sendData))
					{
						std::cout << "发送完成！" << std::endl;
						boost::system::error_code ec;
						write(m_sock, buffer("SendFinish", 10), ec);
						initErrorInfo = false;
						//关闭连接
						CloseSocket();
						std::cout << "断开连接" << m_connId << std::endl;
						if (m_callbackError)
							m_callbackError(m_connId);
					}
					else
					{
						if (strcmp(&(*str)[0], "Ok") == 0)
						{
							//std::cout << "接收消息：" << &(*str)[0] << std::endl;
							offSet = offSet + SEND_SIZE;
							HandleWrite();
						}
						else
						{
							std::cout << "没有接收到返回，重发消息!" << std::endl;
							HandleWrite();
						}
					}
				}
			}
			else
			{
				char command[7] = { 0 };
				char msg[100] = { 0 };
				for (int i = 0; i < 6; i++)
				{
					command[i] = (*str)[i];
					msg[i] = (*str)[i];
				}
				for (int i = 6; i < str->size(); i++)
					msg[i] = (*str)[i];
				if (strcmp(command, "Update") == 0)
				{
					//std::cout << "接收消息：" << command << std::endl;
					std::vector<std::string> vec;
					boost::split(vec, msg, boost::is_any_of("+"));
					appKey = vec[1];
					start_date = vec[2];
					end_date = vec[3];
					httphandler->setAppKey(appKey);
					httphandler->setStartDate(start_date);
					httphandler->setEndDate(end_date);
					string errorList = httphandler->PostHttpRequest();
					setSendData(errorList.c_str());
					httphandler->ParseJsonAndInsertToDatabase();

					offSet = 0;
					initErrorInfo = true;
					HandleWrite();
				}
			}
		}
	}

	void HandleError(const boost::system::error_code& ec)
	{
		CloseSocket();
		std::cout << "断开连接" << m_connId << std::endl;
		//std::cout << ec.message() << std::endl;
		if (m_callbackError)
			m_callbackError(m_connId);
	}

private:
	ip::tcp::socket m_sock;
	std::array<char, MAX_IP_PACK_SIZE> m_buff;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;
	char *sendData = NULL;

	string appKey;
	string start_date;
	string end_date;

	bool requestUpdate;
	bool initErrorInfo;

	deadline_timer m_timer;
	string errorList;
	MyHttpHandler *httphandler;
};