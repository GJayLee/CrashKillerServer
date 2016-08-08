#pragma once

#include<iostream>
#include<unordered_map>
#include<boost\asio.hpp>

#include "MyDatabaseHandler.h"

using namespace boost::asio;
using namespace boost::property_tree;
using boost::asio::ip::tcp;
using std::string;

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
	// �첽д������ɺ�write_handler����
	void write_handler(const boost::system::error_code& ec);
	//�첽��������ɺ�read_handler����
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str);
	//�������Ӵ����쳣
	void HandleError(const boost::system::error_code& ec);
	//ÿ��10s�����ط�
	void wait_handler();

	////����Ŀ�����ļ��л�ȡappkey
	//void InitProjectsTables();
	////�����ݿ��л�ȡ��������Ϣ��ת��ΪJSON��ʽ
	//string GetDeveloperInfo();
	////�����ݿ��л�ȡ���ݲ�������תΪJSON��ʽ��
	//void TransferDataToJson(string appkey);
	////�ӿͻ����յ�������Ϣ���������ݿ�
	////void UpdateDatabase(string crash_id, string developerId, string fixed);
	//void UpdateDatabase(string clientData);
	////���¿����ߵ��쳣��Ϣʱ����Ҫ���¶��쳣���з���
	//void AutoClassifyCrash(string tableName, string developer);
	////�����ݿ���ȡ�����ݣ�δ��������
	//void GetDatabaseData();

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

private:
	ip::tcp::socket m_sock;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;
	//char *sendData = NULL;
	std::unordered_map<int, bool> sendIDArray;
	std::list<int> sendIds;

	//�����ݿ���ȡ�������ݣ�����JSON��ʽ��ȡ
	std::vector<string> crashInfo;
	//�쳣���ݵ�����ID
	int dataToSendIndex;
	//��������Ϣ��JSON��ʽ
	string developerInfo;

	/*std::unordered_map<string, string> appkey_tables;
	std::vector<string> tables;
	string projectConfigure;*/

	bool initErrorInfo;       //�Ƿ����쳣����
	bool initDeveloper;       //�Ƿ��Ϳ�������Ϣ

	deadline_timer m_timer;
	bool isDisConnected;
	bool isReSend;
};