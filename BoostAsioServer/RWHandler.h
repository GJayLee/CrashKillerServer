#pragma once

#include<iostream>
#include<unordered_map>
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

	template<typename F>
	void RWHandler::SetCallBackError(F f)
	{
		m_callbackError = f;
	}

private:
	//从项目配置文件中获取appkey
	void InitProjectsTables();
	
	//从数据库中获取开发者信息并转换为JSON格式
	void GetDeveloperInfo();
	//从数据库中获取数据并把数据转为JSON格式上
	void TransferDataToJson();
	//从客户端收到更新信息，更新数据库
	//void UpdateDatabase(string crash_id, string developerId, string fixed);
	void UpdateDatabase(string clientData);
	//从数据库中取出数据，未处理，测试
	void GetDatabaseData();
	// 异步写操作完成后write_handler触发
	void write_handler(const boost::system::error_code& ec);
	//异步读操作完成后read_handler触发
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str);
	//处理连接错误异常
	void HandleError(const boost::system::error_code& ec);
	//每隔10s调用重发
	void wait_handler();

	//根据传输协议拼接发送消息
	string GetSendData(string flag, string msg);

	//循环使用send ID
	void RecyclSendId(int sendId)
	{
		auto it = sendIDArray.find(sendId);
		if (it != sendIDArray.end())
			sendIDArray.erase(it);
		//std::cout << "current connect count: " << m_handlers.size() << std::endl;
		sendIds.push_back(sendId);
	}

	//把数据写入到文件方便测试
	void writeFile(std::vector<string> res, const char *fileName);
	void writeFile(string res, const char *fileName);

private:
	ip::tcp::socket m_sock;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;
	//char *sendData = NULL;
	std::unordered_map<int, bool> sendIDArray;
	std::list<int> sendIds;

	//连接数据库
	sql::Driver *dirver;
	sql::Connection *con;
	//从数据库中取出的数据，并以JSON格式存取
	std::vector<string> dataInJson;
	//异常数据的索引ID
	int dataToSendIndex;
	//开发者信息，JSON格式
	string developerInfo;

	std::vector<string> tables;

	bool initErrorInfo;       //是否发送异常数据
	bool initDeveloper;       //是否发送开发者信息

	deadline_timer m_timer;
	bool isDisConnected;
	bool isReSend;
};