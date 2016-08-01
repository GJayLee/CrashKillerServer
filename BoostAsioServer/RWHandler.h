#pragma once

#include<iostream>
#include<boost\asio.hpp>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>
#include <boost\serialization\split_member.hpp>
#include<boost\tokenizer.hpp>
#include<boost\algorithm\string.hpp>

#include "MyHttpHandler.h"

using namespace boost::asio;
using namespace boost::property_tree;
using boost::asio::ip::tcp;

class RWHandler
{
public:
	//构造函数
	RWHandler(io_service& ios);
	//析构函数
	~RWHandler();
	//处理读操作
	void HandleRead();
	//处理写操作
	void HandleWrite();
	//获取连接的socket
	ip::tcp::socket& GetSocket();
	//关闭socket
	void CloseSocket();
	//设置当前socket的ID
	void SetConnId(int connId);
	//获取当前socket的ID
	int GetConnId() const;
	//获取项目ID
	string GetAppKey();
	//获取异常数据的开始日期
	string GetStartDate();
	//获取异常数据的结束日期
	string GetEndDate();

	/*void setSendData(const char* str)
	{
		sendData = new char[strlen(str) + 1];
		memset(sendData, 0, sizeof(char)*(strlen(str) + 1));
		strcpy(sendData, str);
		offSet = 0;
	}*/

	template<typename F>
	void RWHandler::SetCallBackError(F f)
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

	//从数据库中获取数据并把数据转为JSON格式上
	void TransferDataToJson();
	//从客户端收到更新信息，更新数据库
	void UpdateDatabase(string crash_id, string developerId, string fixed);
	//从数据库中取出数据，未处理，测试
	void GetDatabaseData();
	// 异步写操作完成后write_handler触发
	void write_handler(const boost::system::error_code& ec);
	//异步读操作完成后read_handler触发
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str);
	//处理连接错误异常
	void HandleError(const boost::system::error_code& ec);
	//把数据写入到文件方便测试
	void writeFile(std::vector<string> res, const char *fileName);

private:
	ip::tcp::socket m_sock;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;
	//char *sendData = NULL;

	//连接数据库
	sql::Driver *dirver;
	sql::Connection *con;
	//从数据库中取出的数据，并以JSON格式存取
	std::vector<string> dataInJson;
	//异常数据的索引ID
	int dataToSendIndex;

	string appKey;
	string start_date;
	string end_date;

	bool initErrorInfo;

	MyHttpHandler *httphandler;
};