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
	//���캯��
	RWHandler(io_service& ios);
	//��������
	~RWHandler();
	//���������
	void HandleRead();
	//����д����
	void HandleWrite();
	//��ȡ���ӵ�socket
	ip::tcp::socket& GetSocket();
	//�ر�socket
	void CloseSocket();
	//���õ�ǰsocket��ID
	void SetConnId(int connId);
	//��ȡ��ǰsocket��ID
	int GetConnId() const;
	//��ȡ��ĿID
	string GetAppKey();
	//��ȡ�쳣���ݵĿ�ʼ����
	string GetStartDate();
	//��ȡ�쳣���ݵĽ�������
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

	//�����ݿ��л�ȡ���ݲ�������תΪJSON��ʽ��
	void TransferDataToJson();
	//�ӿͻ����յ�������Ϣ���������ݿ�
	void UpdateDatabase(string crash_id, string developerId, string fixed);
	//�����ݿ���ȡ�����ݣ�δ��������
	void GetDatabaseData();
	// �첽д������ɺ�write_handler����
	void write_handler(const boost::system::error_code& ec);
	//�첽��������ɺ�read_handler����
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str);
	//�������Ӵ����쳣
	void HandleError(const boost::system::error_code& ec);
	//������д�뵽�ļ��������
	void writeFile(std::vector<string> res, const char *fileName);

private:
	ip::tcp::socket m_sock;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;
	//char *sendData = NULL;

	//�������ݿ�
	sql::Driver *dirver;
	sql::Connection *con;
	//�����ݿ���ȡ�������ݣ�����JSON��ʽ��ȡ
	std::vector<string> dataInJson;
	//�쳣���ݵ�����ID
	int dataToSendIndex;

	string appKey;
	string start_date;
	string end_date;

	bool initErrorInfo;

	MyHttpHandler *httphandler;
};