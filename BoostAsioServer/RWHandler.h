#pragma once

#include<iostream>
#include<unordered_map>
#include<boost\asio.hpp>

#include "MyDatabaseHandler.h"

using namespace boost::asio;
using namespace boost::property_tree;
using boost::asio::ip::tcp;
using std::string;

const int SEND_SIZE = 65535;
const int OFFSET = SEND_SIZE - 2;

class RWHandler:public MyDatabaseHandler
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

	template<typename F>
	void RWHandler::SetCallBackError(F f)
	{
		m_callbackError = f;
	}

private:
	//组装拼接发送数据
	void GenerateSendData(char *data, string str);

	// 异步写操作完成后write_handler触发
	void write_handler(const boost::system::error_code& ec);
	//异步读操作完成后read_handler触发
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str);
	//处理连接错误异常
	void HandleError(const boost::system::error_code& ec);
	//每隔10s调用重发
	void wait_handler();

private:
	ip::tcp::socket m_sock;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;

	//从数据库中取出的数据，并以JSON格式存取
	std::vector<string> crashInfo;
	//异常数据的索引ID
	int dataToSendIndex;
	//开发者信息，JSON格式
	string developerInfo;
	//发送的数据
	char data[SEND_SIZE];

	bool initErrorInfo;       //是否发送异常数据
	bool initDeveloper;       //是否发送开发者信息

	deadline_timer m_timer;
	bool isDisConnected;
	bool isReSend;
};