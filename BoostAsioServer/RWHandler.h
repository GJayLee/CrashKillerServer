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
	//����Ŀ�����ļ��л�ȡappkey
	void InitProjectsTables();
	
	//�����ݿ��л�ȡ��������Ϣ��ת��ΪJSON��ʽ
	void GetDeveloperInfo();
	//�����ݿ��л�ȡ���ݲ�������תΪJSON��ʽ��
	void TransferDataToJson();
	//�ӿͻ����յ�������Ϣ���������ݿ�
	//void UpdateDatabase(string crash_id, string developerId, string fixed);
	void UpdateDatabase(string clientData);
	//�����ݿ���ȡ�����ݣ�δ��������
	void GetDatabaseData();
	// �첽д������ɺ�write_handler����
	void write_handler(const boost::system::error_code& ec);
	//�첽��������ɺ�read_handler����
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str);
	//�������Ӵ����쳣
	void HandleError(const boost::system::error_code& ec);
	//ÿ��10s�����ط�
	void wait_handler();

	//���ݴ���Э��ƴ�ӷ�����Ϣ
	string GetSendData(string flag, string msg);

	//ѭ��ʹ��send ID
	void RecyclSendId(int sendId)
	{
		auto it = sendIDArray.find(sendId);
		if (it != sendIDArray.end())
			sendIDArray.erase(it);
		//std::cout << "current connect count: " << m_handlers.size() << std::endl;
		sendIds.push_back(sendId);
	}

	//������д�뵽�ļ��������
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

	//�������ݿ�
	sql::Driver *dirver;
	sql::Connection *con;
	//�����ݿ���ȡ�������ݣ�����JSON��ʽ��ȡ
	std::vector<string> dataInJson;
	//�쳣���ݵ�����ID
	int dataToSendIndex;
	//��������Ϣ��JSON��ʽ
	string developerInfo;

	std::vector<string> tables;

	bool initErrorInfo;       //�Ƿ����쳣����
	bool initDeveloper;       //�Ƿ��Ϳ�������Ϣ

	deadline_timer m_timer;
	bool isDisConnected;
	bool isReSend;
};