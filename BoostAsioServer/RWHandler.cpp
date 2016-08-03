/*
����������ͻ��˽��ж�д����
*/

#include "RWHandler.h"

//const int MAX_IP_PACK_SIZE = 100;
//const int HEAD_LEN = 1;

const int SEND_SIZE = 65535;

const char STX = '\2';       //���Ŀ�ʼ
const char EOT = '\4';       //���Ľ���
const char ETX = '\3';       //�յ�
const char ENQ = '\5';       //��������

RWHandler::RWHandler(io_service& ios) : m_sock(ios), sendIds(9)
{
	httphandler = new MyHttpHandler();
	offSet = 0;
	dataToSendIndex = 0;
	initErrorInfo = false;
	initDeveloper = false;
	appKey = "";
	start_date = "";
	end_date = "";

	int current = 0;
	std::generate_n(sendIds.begin(), 9, [&current] {return ++current; });

	dirver = get_driver_instance();
	//�������ݿ�
	con = dirver->connect("localhost", "root", "123456");
	//ѡ��mydata���ݿ�
	con->setSchema("CrashKiller");
}

RWHandler::~RWHandler()
{
	delete con;
}

void RWHandler::HandleRead()
{
	//��������»᷵�أ�1.����������2.transfer_at_leastΪ��(�յ��ض������ֽڼ�����)��3.�д�����
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
	/*boost::system::error_code ec;
	write(m_sock, buffer(data, len), ec);
	if (ec != nullptr)
	HandleError(ec);*/
	// ������Ϣ(������)
	/*boost::shared_ptr<std::string> pstr(new
	std::string("\4asdqwdqwddascd"));
	m_sock.async_write_some(buffer(*pstr),
	boost::bind(&RWHandler::write_handler, this, placeholders::error));*/

	if (initErrorInfo)
	{
		/*std::stringstream ss;
		string dataSize;
		ss << dataInJson[dataToSendIndex].size();
		ss >> dataSize;
		string data = dataSize + "+" + dataInJson[dataToSendIndex];*/

		//û�г�����󻺳�����������������
		if (dataInJson[dataToSendIndex].size() <= SEND_SIZE)
		{
			//��ӽ��������߿ͻ������Ǹ�data�����һ��
			string sendStr = GetSendData(EOT, dataInJson[dataToSendIndex]);
			m_sock.async_write_some(buffer(sendStr.c_str(), sendStr.size()),
				boost::bind(&RWHandler::write_handler, this, placeholders::error));
		}
		else
		{
			/*string endStr = "End" + dataInJson[dataToSendIndex];
			m_sock.async_write_some(buffer(endStr.c_str() + offSet, dataInJson[dataToSendIndex].size() - offSet + 3),
			boost::bind(&RWHandler::write_handler, this, placeholders::error));*/
			//�����ݽضϷ���
			//��ӽ��������߿ͻ������Ǹ�data�����һ��
			if ((offSet + SEND_SIZE) >= dataInJson[dataToSendIndex].size())
			{
				string tempStr = dataInJson[dataToSendIndex].substr(offSet, dataInJson[dataToSendIndex].size() - offSet);
				string sendStr = GetSendData(EOT, tempStr);
				m_sock.async_write_some(buffer(sendStr, sendStr.size()),
					boost::bind(&RWHandler::write_handler, this, placeholders::error));
				
			}
			else
			{
				string tempStr = dataInJson[dataToSendIndex].substr(offSet, SEND_SIZE);
				string sendStr = GetSendData(STX, tempStr);
				m_sock.async_write_some(buffer(sendStr, sendStr.size()),
					boost::bind(&RWHandler::write_handler, this, placeholders::error));
			}
		}

		/*int sendSize = 0;
		if (strlen(sendData) - offSet < SEND_SIZE)
		sendSize = strlen(sendData) - offSet;
		else
		sendSize = SEND_SIZE;
		m_sock.async_write_some(buffer(sendData + offSet, sendSize),
		boost::bind(&RWHandler::write_handler, this, placeholders::error));*/
	}
	//���Ϳ�������Ϣ
	if (initDeveloper)
	{
		//û�г�����󻺳�����������������
		if (developerInfo.size() <= SEND_SIZE)
		{
			//��ӽ��������߿ͻ������Ǹ�data�����һ��
			string sendStr = GetSendData(EOT, developerInfo);
			m_sock.async_write_some(buffer(sendStr.c_str(), sendStr.size()),
				boost::bind(&RWHandler::write_handler, this, placeholders::error));
		}
		else
		{
			//�����ݽضϷ���
			//��ӽ��������߿ͻ������Ǹ�data�����һ��
			if ((offSet + SEND_SIZE) >= developerInfo.size())
			{
				string tempStr = developerInfo.substr(offSet, developerInfo.size() - offSet);
				string sendStr = GetSendData(EOT, tempStr);
				m_sock.async_write_some(buffer(sendStr, sendStr.size()),
					boost::bind(&RWHandler::write_handler, this, placeholders::error));
			}
			else
			{
				string tempStr = developerInfo.substr(offSet, SEND_SIZE);
				string sendStr = GetSendData(STX, tempStr);
				m_sock.async_write_some(buffer(sendStr, sendStr.size()),
					boost::bind(&RWHandler::write_handler, this, placeholders::error));
			}
		}
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

string RWHandler::GetAppKey()
{
	return appKey;
}

string RWHandler::GetStartDate()
{
	return start_date;
}

string RWHandler::GetEndDate()
{
	return end_date;
}

/*void setSendData(const char* str)
{
sendData = new char[strlen(str) + 1];
memset(sendData, 0, sizeof(char)*(strlen(str) + 1));
strcpy(sendData, str);
offSet = 0;
}*/

//�����ݿ��л�ȡ��������Ϣ��ת��ΪJSON��ʽ
void RWHandler::GetDeveloperInfo()
{
	sql::Statement *stmt;
	sql::ResultSet *res;

	stmt = con->createStatement();
	developerInfo = "";
	//��developer���л�ȡ������Ϣ
	res = stmt->executeQuery("select * from developer");
	std::stringstream stream;
	ptree pt, pt2;
	//ѭ������
	while (res->next())
	{
		ptree pt1;
		//��ȡ������Ϣת��ΪJSON��ʽ
		pt1.put("Id", res->getString("ID"));
		pt1.put("Name", res->getString("Name"));
		pt2.push_back(make_pair("", pt1));
	}

	pt.put_child("developer", pt2);
	write_json(stream, pt);
	developerInfo = stream.str();

	writeFile(developerInfo, "developInfo.txt");

	delete stmt;
	delete res;
}

//�����ݿ��л�ȡ���ݲ�������תΪJSON��ʽ��
void RWHandler::TransferDataToJson()
{
	dataInJson.clear();

	sql::Statement *stmt;
	sql::ResultSet *res;

	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";
	//��errorinfo���л�ȡ������Ϣ
	res = stmt->executeQuery("select * from errorinfo");

	//ѭ������
	while (res->next())
	{
		//��ȡ������Ϣת��ΪJSON��ʽ
		std::stringstream stream;
		ptree pt, pt1, pt2;
		pt1.put("crash_id", res->getString("crash_id"));
		pt1.put("developer", res->getString("developer"));
		pt1.put("fixed", res->getString("fixed"));
		pt1.put("app_version", res->getString("app_version"));
		pt1.put("first_crash_date_time", res->getString("first_crash_date_time"));
		pt1.put("last_crash_date_time", res->getString("last_crash_date_time"));
		pt1.put("crash_context_digest", res->getString("crash_context_digest"));
		pt1.put("crash_context", res->getString("crash_context"));

		pt2.push_back(make_pair("", pt1));
		pt.put_child("data", pt2);
		write_json(stream, pt);
		dataInJson.push_back(stream.str());
	}

	//writeFile(dataInJson, "dataJson.txt");

	//����
	delete res;
	delete stmt;
}

//�ӿͻ����յ�������Ϣ���������ݿ�
void RWHandler::UpdateDatabase(string crash_id, string developerId, string fixed)
{
	sql::Statement *stmt;

	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();
	string sqlStateMent = "UPDATE errorinfo SET developerid = \"" + developerId
		+ "\",fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
	stmt->execute(sqlStateMent);

	delete stmt;
}

//�����ݿ���ȡ�����ݣ�δ��������
void RWHandler::GetDatabaseData()
{
	sql::Driver *dirver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;
	dirver = get_driver_instance();
	//�������ݿ�
	con = dirver->connect("localhost", "root", "123456");
	//ѡ��mydata���ݿ�
	con->setSchema("CrashKiller");
	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";

	//��name_table���л�ȡ������Ϣ
	res = stmt->executeQuery("select * from errorinfo");
	//ѭ������
	while (res->next())
	{
		//�����id��name��age,work,others�ֶε���Ϣ
		//cout << res->getString("name") << " | " << res->getInt("age") << endl;
		result = result + res->getString("ID") + res->getString("crash_id")
			+ res->getString("fixed") + res->getString("app_version")
			+ res->getString("first_crash_date_time") + res->getString("crash_context_digest")
			+ res->getString("crash_context");
	}

	//setSendData(result.c_str());

	//����
	delete res;
	delete stmt;
	delete con;
}

// �첽д������ɺ�write_handler����
//void write_handler(const boost::system::error_code& ec, boost::shared_ptr<std::string> str)
void RWHandler::write_handler(const boost::system::error_code& ec)
{
	if (ec)
	{
		std::cout << "����ʧ��!" << std::endl;
		HandleError(ec);
	}
	else
	{
		//std::cout << "�ɹ����ͣ�" << std::endl;
		//������Ϣ����������
		HandleRead();
	}

}
//�첽��������ɺ�read_handler����
void RWHandler::read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str)
{
	if (ec)
	{
		//std::cout << "û�н��յ���Ϣ��" << std::endl;
		HandleError(ec);
		return;
	}
	else
	{
		//�ͻ��������ʼ���쳣����ʱ����
		//if (initErrorInfo || strcmp(&(*str)[0], "Get Expect") == 0)
		if (initErrorInfo || strcmp(&(*str)[2], "Get Expect") == 0)
		{
			//HandleWrite();
			if (!initErrorInfo)
			{
				initErrorInfo = true;
				//GetDatabaseData();
				TransferDataToJson();
				dataToSendIndex = 0;
				offSet = 0;
				HandleWrite();
			}
			else
			{
				//if (strcmp(&(*str)[0], ACK) == 0)
				if ((*str)[0] == ETX)
				{
					int sendId = (*str)[1] - '0';
					RecyclSendId(sendId);
					if ((offSet + SEND_SIZE) > dataInJson[dataToSendIndex].size())
					{
						//std::cout << (*str)[1] << "������ɣ�" << std::endl;
						std::cout << dataToSendIndex << "������ɣ�" << std::endl;
						++dataToSendIndex;
						//�ж��Ƿ�����һ���쳣���������������ͣ�����ر�����
						if (dataToSendIndex < dataInJson.size())
						{
							offSet = 0;
							HandleWrite();
						}
						else
						{
							boost::system::error_code ec;
							string sendStr = GetSendData(EOT,"SendFinish");
							write(m_sock, buffer(sendStr, sendStr.size()), ec);
							initErrorInfo = false;

							//HandleRead();
							////�ر�����
							//CloseSocket();
							//std::cout << "�Ͽ�����" << m_connId << std::endl;
							//if (m_callbackError)
							//	m_callbackError(m_connId);
						}
					}
					else
					{
						//std::cout << "������Ϣ��" << &(*str)[0] << std::endl;
						offSet = offSet + SEND_SIZE;
						HandleWrite();
					}
				}
				else
				{
					std::cout << "û�н��յ����أ��ط���Ϣ!" << std::endl;
					HandleWrite();
				}
			}
		}
		//�ͻ��˷��ͻ�ȡ��������Ϣʱ
		else if (initDeveloper || strcmp(&(*str)[2], "Get Develop") == 0)
		{
			if (!initDeveloper)
			{
				initDeveloper = true;
				GetDeveloperInfo();
				offSet = 0;
				HandleWrite();
			}
			else
			{
				//if (strcmp(&(*str)[0], "Ok") == 0)
				if ((*str)[0] == ETX)
				{
					if ((offSet + SEND_SIZE) > developerInfo.size())
					{
						std::cout << "��������Ϣ������ɣ�" << std::endl;
						boost::system::error_code ec;
						string sendStr = GetSendData(EOT, "SendFinish");
						write(m_sock, buffer(sendStr, sendStr.size()), ec);
						initDeveloper = false;
					}
					else
					{
						offSet = offSet + SEND_SIZE;
						HandleWrite();
					}
				}
				else
				{
					std::cout << "û�н��յ����أ��ط���Ϣ!" << std::endl;
					HandleWrite();
				}
			}
		}
		//�ͻ��˷��ظ������ݣ��������ĸ��û����쳣�Ƿ��ѽ��ʱ����
		else if (strcmp(&(*str)[0], "Update") == 0)
		{
			UpdateDatabase("0533e5d3-7bf6-4a75-95c9-b5b3b3264880", "18520147781", "false");
			HandleRead();
		}
		//�������������
		else
		{
			std::cout << "������Ϣ��" << &(*str)[0] << std::endl;
			HandleRead();
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
				//std::cout << "������Ϣ��" << command << std::endl;
				std::vector<std::string> vec;
				boost::split(vec, msg, boost::is_any_of("+"));
				appKey = vec[1];
				start_date = vec[2];
				end_date = vec[3];
				httphandler->setAppKey(appKey);
				httphandler->setStartDate(start_date);
				httphandler->setEndDate(end_date);
				httphandler->PostHttpRequest();
				//setSendData(errorList.c_str());
				httphandler->ParseJsonAndInsertToDatabase();

				offSet = 0;
				initErrorInfo = true;
				HandleWrite();
			}
		}
	}
}

//���ݴ���Э��ƴ�ӷ�����Ϣ     head+count+data
string RWHandler::GetSendData(const char flag, string msg)
{
	int sendId = sendIds.front();
	sendIds.pop_front();
	std::stringstream ss;
	ss << sendId;
	string indexStr = ss.str();
	sendIDArray[sendId] = true;

	string data = flag + indexStr + msg;

	//ss.clear();

	return data;
}

void RWHandler::HandleError(const boost::system::error_code& ec)
{
	CloseSocket();
	std::cout << "�Ͽ�����" << m_connId << std::endl;
	//std::cout << ec.message() << std::endl;
	if (m_callbackError)
		m_callbackError(m_connId);
}

//������д�뵽�ļ��������
void RWHandler::writeFile(std::vector<string> res, const char *fileName)
{
	std::ofstream out(fileName, std::ios::out);
	if (!out)
	{
		std::cout << "Can not Open this file!" << std::endl;
		return;
	}
	
	std::stringstream stream;
	ptree pt, pt1;
	for (int i = 0; i < res.size(); i++)
	{
		/*out << res[i];
		out << std::endl;*/
		//ptree pt1;
		pt1.put("", res[i]);

		pt.push_back(make_pair("", pt1));
	}
	//pt.put_child("datas", pt2);
	write_json(stream, pt);

	out << stream.str();

	out.close();
}

//������д�뵽�ļ��������
void RWHandler::writeFile(string res, const char *fileName)
{
	std::ofstream out(fileName, std::ios::out);
	if (!out)
	{
		std::cout << "Can not Open this file!" << std::endl;
		return;
	}
	out << res;
	out << std::endl;

	out.close();
}