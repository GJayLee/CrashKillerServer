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

	template<typename F>
	void RWHandler::SetCallBackError(F f)
	{
		m_callbackError = f;
	}

private:
	//��װƴ�ӷ�������
	void GenerateSendData(char *data, string str);

	// �첽д������ɺ�write_handler����
	void write_handler(const boost::system::error_code& ec);
	//�첽��������ɺ�read_handler����
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str);
	//�������Ӵ����쳣
	void HandleError(const boost::system::error_code& ec);
	//ÿ��10s�����ط�
	void wait_handler();

private:
	ip::tcp::socket m_sock;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;

	//�����ݿ���ȡ�������ݣ�����JSON��ʽ��ȡ
	std::vector<string> crashInfo;
	//�쳣���ݵ�����ID
	int dataToSendIndex;
	//��������Ϣ��JSON��ʽ
	string developerInfo;
	//���͵�����
	char data[SEND_SIZE];

	bool initErrorInfo;       //�Ƿ����쳣����
	bool initDeveloper;       //�Ƿ��Ϳ�������Ϣ

	deadline_timer m_timer;
	bool isDisConnected;
	bool isReSend;
};