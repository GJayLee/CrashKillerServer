/*
处理服务端与客户端进行读写操作
*/

#include <unordered_set>
#include <boost\regex.hpp>

#include "RWHandler.h"
#include "generic.h"

//传输协议的头部列表
const string STX = "\x2";       //正文开始
const string EOT = "\x4";       //正文结束
const string ETX = "\x3";       //收到
//客户端请求命令列表
const char* INIT_APPKEYS_INFO = "Get Appkeys";        //初始化项目信息的请求命令
const char* INIT_CRASH_INFO = "Get Expect";           //初始化异常信息的请求命令
const char* INIT_DEVELOPER_INFO = "Get Develop";      //初始化开发者信息的请求命令
const char* UPDATE_CRASH_INFO = "Up";                 //请求更新的异常信息的命令

RWHandler::RWHandler(io_service& ios) : m_sock(ios), m_timer(ios)
{
	offSet = 0;
	dataToSendIndex = 0;
	initErrorInfo = false;
	initDeveloper = false;

	isDisConnected = false;

	memset(data, 0, SEND_SIZE);

	//开启异步等待
	m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));
}

RWHandler::~RWHandler()
{
}

/*
header:
STX         正文开始
EOT         正文结束
ETX         收到
*/
//组装拼接发送数据
void RWHandler::GenerateSendData(char *data, string str)
{
	string sendStr;
	//没有超过最大缓冲区，可以完整发送
	if (str.size() <= SEND_SIZE)
	{
		sendStr = EOT + "\x1" + str;
		strcpy(data, sendStr.c_str());
		for (int i = sendStr.size(); i < SEND_SIZE; i++)
			data[i] = ' ';
	}
	else
	{
		//把数据截断发送
		if ((offSet + OFFSET) >= str.size())
		{
			//添加结束符告诉客户端这是该data的最后一段
			string tempStr = str.substr(offSet, str.size() - offSet);
			sendStr = EOT + "\x1" + tempStr;
			strcpy(data, sendStr.c_str());
			for (int i = sendStr.size(); i < SEND_SIZE; i++)
				data[i] = ' ';
		}
		else
		{
			string tempStr = str.substr(offSet, OFFSET);
			sendStr = STX + "\x1" + tempStr;
			strcpy(data, sendStr.c_str());
		}
	}
}

void RWHandler::HandleRead()
{
	try
	{
		//接受消息（非阻塞）
		boost::shared_ptr<std::vector<char> > str(new std::vector<char>(200, 0));
		m_sock.async_read_some(buffer(*str), bind(&RWHandler::read_handler, this, placeholders::error, str));
	}
	catch (std::exception& e)
	{
		std::cout << "读取错误" << std::endl;
	}
}

void RWHandler::HandleWrite()
{
	// 发送信息(非阻塞)
	if (initErrorInfo)
	{
		memset(data, 0, SEND_SIZE);
		GenerateSendData(data, crashInfo[dataToSendIndex]);
		m_sock.async_write_some(buffer(data, SEND_SIZE),
			boost::bind(&RWHandler::write_handler, this, placeholders::error));
		//从发送起计算，10s后没接收到返回就重发
		isReSend = true;
		m_timer.expires_from_now(boost::posix_time::seconds(10));
	}
	//发送开发者信息
	if (initDeveloper)
	{
		memset(data, 0, SEND_SIZE);
		GenerateSendData(data, developerInfo);
		m_sock.async_write_some(buffer(data, SEND_SIZE),
			boost::bind(&RWHandler::write_handler, this, placeholders::error));
		//从发送起计算，10s后没接收到返回就重发
		isReSend = true;
		m_timer.expires_from_now(boost::posix_time::seconds(10));
	}
}

ip::tcp::socket& RWHandler::GetSocket()
{
	return m_sock;
}

void RWHandler::CloseSocket()
{
	boost::system::error_code ec;
	m_sock.shutdown(ip::tcp::socket::shutdown_send, ec);
	m_sock.close(ec);
}

void RWHandler::SetConnId(int connId)
{
	m_connId = connId;
}

int RWHandler::GetConnId() const
{
	return m_connId;
}

// 异步写操作完成后write_handler触发
void RWHandler::write_handler(const boost::system::error_code& ec)
{
	if (ec)
	{
		//std::cout << "send" << std::endl;
		HandleError(ec);
	}
	else
	{
		//接受消息（非阻塞）
		HandleRead();
	}

}
//异步读操作完成后read_handler触发
void RWHandler::read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str)
{
	if (ec)
	{
		//std::cout << "read" << dataToSendIndex << std::endl;
		HandleError(ec);
		return;
	}
	else
	{
		//从接收到的信息中获取请求命令
		char command[30] = { 0 };
		int i;
		for (i = 2; i < 30; i++)
		{
			if ((*str)[i] == '|')
				break;
			command[i - 2] = (*str)[i];
		}
		//客户端请求初始化项目信息
		if (strcmp(&(*str)[2], INIT_APPKEYS_INFO) == 0)
		{
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1];
			write(m_sock, buffer(sendStr, sendStr.size()), ec);
			Sleep(500);

			memset(data, 0, SEND_SIZE);
			GenerateSendData(data, projectConfigureInfoInJson);
			write(m_sock, buffer(data, SEND_SIZE), ec);
			std::cout << "ProjectConfigure finish!" << std::endl;

			HandleRead();
		}
		//客户端请求初始化异常数据时调用
		else if (strcmp(command, INIT_CRASH_INFO) == 0)
		{
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1];
			write(m_sock, buffer(sendStr, sendStr.size()), ec);
			Sleep(500);

			initErrorInfo = true;
			TransferDataToJson(crashInfo, &(*str)[i + 1]);
			dataToSendIndex = 0;
			offSet = 0;
			HandleWrite();
		}
		//客户端发送获取开发者信息时
		else if (strcmp(&(*str)[2], INIT_DEVELOPER_INFO) == 0)
		{
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1];
			write(m_sock, buffer(sendStr, sendStr.size()), ec);

			Sleep(500);

			initDeveloper = true;
			GetDeveloperInfo(developerInfo);
			offSet = 0;
			HandleWrite();
		}
		//收到确认返回时，根据返回执行不同的操作
		else if ((*str)[0] == ETX[0])
		{
			//收到返回，设置重发为false
			isReSend = false;
			if (initErrorInfo)
			{
				//int sendId = (*str)[1] - '0';
				if ((offSet + OFFSET) > crashInfo[dataToSendIndex].size())
				{
					std::cout << dataToSendIndex << "发送完成！" << std::endl;
					++dataToSendIndex;
					//判断是否还有下一条异常，如果有则继续发送，无则关闭连接
					if (dataToSendIndex < crashInfo.size())
					{
						offSet = 0;
						HandleWrite();
					}
					else
					{
						boost::system::error_code ec;
						memset(data, 0, SEND_SIZE);
						GenerateSendData(data, "SendFinish");
						write(m_sock, buffer(data, SEND_SIZE), ec);
						initErrorInfo = false;
					}
				}
				else
				{
					offSet = offSet + OFFSET;
					HandleWrite();
				}
			}
			else if (initDeveloper)
			{
				//int sendId = (*str)[1] - '0';
				if ((offSet + OFFSET) > developerInfo.size())
				{
					std::cout << "开发者信息发送完成！" << std::endl;
					boost::system::error_code ec;
					memset(data, 0, SEND_SIZE);
					GenerateSendData(data, "SendFinish");
					write(m_sock, buffer(data, SEND_SIZE), ec);
					initDeveloper = false;
				}
				else
				{
					offSet = offSet + OFFSET;
					HandleWrite();
				}
			}
			HandleRead();
		}
		//客户端返回更新内容，如分配给哪个用户，异常是否已解决时调用
		else if(strcmp(command, UPDATE_CRASH_INFO) == 0)
		{
			std::cout << "接收消息：" << &(*str)[0] << std::endl;
			UpdateDatabase(&(*str)[i + 1]);
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1];
			write(m_sock, buffer(sendStr, sendStr.size()), ec);
			HandleRead();
		}
		//其他情况
		else
		{
			std::cout << "接收消息：" << &(*str)[0] << std::endl;
			HandleRead();
		}
	}
}

void RWHandler::HandleError(const boost::system::error_code& ec)
{
	if (!isDisConnected)
	{
		m_timer.cancel();
		//m_timer.expires_at(boost::posix_time::pos_infin);
		CloseSocket();
		std::cout << "断开连接" << m_connId << std::endl;
		//std::cout << ec.message() << std::endl;
		if (m_callbackError)
			m_callbackError(m_connId);
	}
	
	isDisConnected = true;
}

//等待10s，如果没收到返回，即isReSend为true则调用重发
void RWHandler::wait_handler()
{
	if (isDisConnected)
		return;

	if (m_timer.expires_at() <= deadline_timer::traits_type::now()&&isReSend)
	{
		std::cout << "没有接收到返回，重发消息!" << std::endl;
		if (m_sock.is_open())
			HandleWrite();
		else
			std::cout << "重发失败，连接已断开！" << std::endl;
	}
	m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));
}