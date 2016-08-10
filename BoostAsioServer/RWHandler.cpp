/*
����������ͻ��˽��ж�д����
*/

#include <unordered_set>
#include <boost\regex.hpp>

#include "RWHandler.h"
#include "generic.h"

//����Э���ͷ���б�
const string STX = "\x2";       //���Ŀ�ʼ
const string EOT = "\x4";       //���Ľ���
const string ETX = "\x3";       //�յ�
//�ͻ������������б�
const char* INIT_APPKEYS_INFO = "Get Appkeys";        //��ʼ����Ŀ��Ϣ����������
const char* INIT_CRASH_INFO = "Get Expect";           //��ʼ���쳣��Ϣ����������
const char* INIT_DEVELOPER_INFO = "Get Develop";      //��ʼ����������Ϣ����������
const char* UPDATE_CRASH_INFO = "Up";                 //������µ��쳣��Ϣ������

RWHandler::RWHandler(io_service& ios) : m_sock(ios), m_timer(ios)
{
	offSet = 0;
	dataToSendIndex = 0;
	initErrorInfo = false;
	initDeveloper = false;

	isDisConnected = false;

	memset(data, 0, SEND_SIZE);

	//�����첽�ȴ�
	m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));
}

RWHandler::~RWHandler()
{
}

/*
header:
STX         ���Ŀ�ʼ
EOT         ���Ľ���
ETX         �յ�
*/
//��װƴ�ӷ�������
void RWHandler::GenerateSendData(char *data, string str)
{
	string sendStr;
	//û�г�����󻺳�����������������
	if (str.size() <= SEND_SIZE)
	{
		sendStr = EOT + "\x1" + str;
		strcpy(data, sendStr.c_str());
		for (int i = sendStr.size(); i < SEND_SIZE; i++)
			data[i] = ' ';
	}
	else
	{
		//�����ݽضϷ���
		if ((offSet + OFFSET) >= str.size())
		{
			//��ӽ��������߿ͻ������Ǹ�data�����һ��
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
		//������Ϣ����������
		boost::shared_ptr<std::vector<char> > str(new std::vector<char>(200, 0));
		m_sock.async_read_some(buffer(*str), bind(&RWHandler::read_handler, this, placeholders::error, str));
	}
	catch (std::exception& e)
	{
		std::cout << "��ȡ����" << std::endl;
	}
}

void RWHandler::HandleWrite()
{
	// ������Ϣ(������)
	if (initErrorInfo)
	{
		memset(data, 0, SEND_SIZE);
		GenerateSendData(data, crashInfo[dataToSendIndex]);
		m_sock.async_write_some(buffer(data, SEND_SIZE),
			boost::bind(&RWHandler::write_handler, this, placeholders::error));
		//�ӷ�������㣬10s��û���յ����ؾ��ط�
		isReSend = true;
		m_timer.expires_from_now(boost::posix_time::seconds(10));
	}
	//���Ϳ�������Ϣ
	if (initDeveloper)
	{
		memset(data, 0, SEND_SIZE);
		GenerateSendData(data, developerInfo);
		m_sock.async_write_some(buffer(data, SEND_SIZE),
			boost::bind(&RWHandler::write_handler, this, placeholders::error));
		//�ӷ�������㣬10s��û���յ����ؾ��ط�
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

// �첽д������ɺ�write_handler����
void RWHandler::write_handler(const boost::system::error_code& ec)
{
	if (ec)
	{
		//std::cout << "send" << std::endl;
		HandleError(ec);
	}
	else
	{
		//������Ϣ����������
		HandleRead();
	}

}
//�첽��������ɺ�read_handler����
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
		//�ӽ��յ�����Ϣ�л�ȡ��������
		char command[30] = { 0 };
		int i;
		for (i = 2; i < 30; i++)
		{
			if ((*str)[i] == '|')
				break;
			command[i - 2] = (*str)[i];
		}
		//�ͻ��������ʼ����Ŀ��Ϣ
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
		//�ͻ��������ʼ���쳣����ʱ����
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
		//�ͻ��˷��ͻ�ȡ��������Ϣʱ
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
		//�յ�ȷ�Ϸ���ʱ�����ݷ���ִ�в�ͬ�Ĳ���
		else if ((*str)[0] == ETX[0])
		{
			//�յ����أ������ط�Ϊfalse
			isReSend = false;
			if (initErrorInfo)
			{
				//int sendId = (*str)[1] - '0';
				if ((offSet + OFFSET) > crashInfo[dataToSendIndex].size())
				{
					std::cout << dataToSendIndex << "������ɣ�" << std::endl;
					++dataToSendIndex;
					//�ж��Ƿ�����һ���쳣���������������ͣ�����ر�����
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
					std::cout << "��������Ϣ������ɣ�" << std::endl;
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
		//�ͻ��˷��ظ������ݣ��������ĸ��û����쳣�Ƿ��ѽ��ʱ����
		else if(strcmp(command, UPDATE_CRASH_INFO) == 0)
		{
			std::cout << "������Ϣ��" << &(*str)[0] << std::endl;
			UpdateDatabase(&(*str)[i + 1]);
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1];
			write(m_sock, buffer(sendStr, sendStr.size()), ec);
			HandleRead();
		}
		//�������
		else
		{
			std::cout << "������Ϣ��" << &(*str)[0] << std::endl;
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
		std::cout << "�Ͽ�����" << m_connId << std::endl;
		//std::cout << ec.message() << std::endl;
		if (m_callbackError)
			m_callbackError(m_connId);
	}
	
	isDisConnected = true;
}

//�ȴ�10s�����û�յ����أ���isReSendΪtrue������ط�
void RWHandler::wait_handler()
{
	if (isDisConnected)
		return;

	if (m_timer.expires_at() <= deadline_timer::traits_type::now()&&isReSend)
	{
		std::cout << "û�н��յ����أ��ط���Ϣ!" << std::endl;
		if (m_sock.is_open())
			HandleWrite();
		else
			std::cout << "�ط�ʧ�ܣ������ѶϿ���" << std::endl;
	}
	m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));
}