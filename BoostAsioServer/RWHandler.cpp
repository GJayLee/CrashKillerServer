/*
����������ͻ��˽��ж�д����
*/

#include <unordered_set>
#include <boost\regex.hpp>

#include "RWHandler.h"
#include "generic.h"

const int SEND_SIZE = 65535;

const string STX = "\x2";       //���Ŀ�ʼ
const string EOT = "\x4";       //���Ľ���
const string ETX = "\x3";       //�յ�

const char* INIT_CRASH_INFO = "Get Expect";           //��ʼ���쳣��Ϣ����������
const char* INIT_DEVELOPER_INFO = "Get Develop";      //��ʼ����������Ϣ����������
const char* UPDATE_CRASH_INFO = "Up";                 //������µ��쳣��Ϣ������

RWHandler::RWHandler(io_service& ios) : m_sock(ios), sendIds(9), m_timer(ios)
{
	offSet = 0;
	dataToSendIndex = 0;
	initErrorInfo = false;
	initDeveloper = false;

	isDisConnected = false;

	int current = 0;
	std::generate_n(sendIds.begin(), 9, [&current] {return ++current; });

	dirver = get_driver_instance();
	//�������ݿ�
	con = dirver->connect("localhost", "root", "123456");
	//ѡ��mydata���ݿ�
	con->setSchema("CrashKiller");

	InitProjectsTables();

	//�����첽�ȴ�
	m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));
}

RWHandler::~RWHandler()
{
	delete con;
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
		//û�г�����󻺳�����������������
		if (dataInJson[dataToSendIndex].size() <= SEND_SIZE)
		{
			std::cout << "����" << dataToSendIndex << std::endl;
			//��ӽ��������߿ͻ������Ǹ�data�����һ��
			string sendStr = GetSendData(EOT, dataInJson[dataToSendIndex]);
			m_sock.async_write_some(buffer(sendStr.c_str(), sendStr.size()),
				boost::bind(&RWHandler::write_handler, this, placeholders::error));
		}
		else
		{
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
		//�ӷ�������㣬10s��û���յ����ؾ��ط�
		isReSend = true;
		m_timer.expires_from_now(boost::posix_time::seconds(10));
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
		std::cout << "����ʧ��!" << std::endl;
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
		//std::cout << "û�н��յ���Ϣ��" << std::endl;
		HandleError(ec);
		return;
	}
	else
	{
		//�ӽ��յ�����Ϣ�л�ȡ��������
		char command[30] = { 0 };
		for (int i = 2; i < 30; i++)
		{
			if ((*str)[i] == '\0')
				break;
			command[i - 2] = (*str)[i];
		}
		//�ͻ��������ʼ���쳣����ʱ����
		if (initErrorInfo || strcmp(&(*str)[2], INIT_CRASH_INFO) == 0)
		//if (strcmp(command, "Get Expect") == 0)
		{
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1] + projectConfigure;
			write(m_sock, buffer(sendStr, sendStr.size()), ec);

			Sleep(500);

			initErrorInfo = true;
			TransferDataToJson(&(*str)[13]);
			dataToSendIndex = 0;
			offSet = 0;
			HandleWrite();
		}
		//�ͻ��˷��ͻ�ȡ��������Ϣʱ
		else if (strcmp(&(*str)[2], INIT_DEVELOPER_INFO) == 0)
		{
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1] + "Receive";
			write(m_sock, buffer(sendStr, sendStr.size()), ec);

			Sleep(500);

			initDeveloper = true;
			GetDeveloperInfo();
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
				int sendId = (*str)[1] - '0';
				//RecyclSendId(sendId);
				if ((offSet + SEND_SIZE) > dataInJson[dataToSendIndex].size())
				{
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
						string sendStr = GetSendData(EOT, "SendFinish");
						write(m_sock, buffer(sendStr, sendStr.size()), ec);
						initErrorInfo = false;

						HandleRead();
					}
				}
				else
				{
					offSet = offSet + SEND_SIZE;
					HandleWrite();
				}
			}
			else if (initDeveloper)
			{
				int sendId = (*str)[1] - '0';
				//RecyclSendId(sendId);
				if ((offSet + SEND_SIZE) > developerInfo.size())
				{
					std::cout << "��������Ϣ������ɣ�" << std::endl;
					boost::system::error_code ec;
					string sendStr = GetSendData(EOT, "SendFinish");
					write(m_sock, buffer(sendStr, sendStr.size()), ec);
					initDeveloper = false;
					HandleRead();
				}
				else
				{
					offSet = offSet + SEND_SIZE;
					HandleWrite();
				}
			}
		}
		//�ͻ��˷��ظ������ݣ��������ĸ��û����쳣�Ƿ��ѽ��ʱ����
		else if(strcmp(command, UPDATE_CRASH_INFO) == 0)
		{
			std::cout << "������Ϣ��" << &(*str)[0] << std::endl;
			UpdateDatabase(&(*str)[4]);
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1] + "UpdateFinish";
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

//���ݴ���Э��ƴ�ӷ�����Ϣ     head+count+data
string RWHandler::GetSendData(string flag, string msg)
{
	/*int sendId = sendIds.front();
	sendIds.pop_front();
	std::stringstream ss;
	ss << sendId;
	string indexStr = ss.str();
	sendIDArray[sendId] = true;*/

	string data = flag + "\x1" + msg;

	//ss.clear();

	return data;
}

void RWHandler::HandleError(const boost::system::error_code& ec)
{
	m_timer.cancel();
	isDisConnected = true;
	//m_timer.expires_at(boost::posix_time::pos_infin);
	CloseSocket();
	std::cout << "�Ͽ�����" << m_connId << std::endl;
	//std::cout << ec.message() << std::endl;
	if (m_callbackError)
		m_callbackError(m_connId);
}

//�ȴ�10s�����û�յ����أ���isReSendΪtrue������ط�
void RWHandler::wait_handler()
{
	if (isDisConnected)
		return;

	if (m_timer.expires_at() <= deadline_timer::traits_type::now()&&isReSend)
	{
		std::cout << "û�н��յ����أ��ط���Ϣ!" << std::endl;
		HandleWrite();
	}
	m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));
}

//����Ŀ�����ļ��л�ȡappkey
void RWHandler::InitProjectsTables()
{
	//��ȡ�����ļ���appkey��Ϣ
	std::ifstream t("ProjectsConfigure.txt");
	if (!t)
	{
		std::cout << "����ĿAppkey�����ļ�ʧ�ܣ�" << std::endl;
		return;
	}
	std::stringstream buffer;
	buffer << t.rdbuf();
	//��ʼ����Ŀ������Ϣ
	projectConfigure = buffer.str();

	ptree pt, pt1, pt2;
	read_json<ptree>(buffer, pt);
	pt1 = pt.get_child("projects");
	for (ptree::iterator it = pt1.begin(); it != pt1.end(); ++it)
	{
		pt2 = it->second;
		appkey_tables[pt2.get<string>("appkey")] = pt2.get<string>("tableName");
		tables.push_back(pt2.get<string>("tableName"));
	}
}

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

//�����ݿ��л�ȡ���ݲ�������תΪJSON��ʽ��, �˴�Ŀǰ��Ϊֻ��һ��appkey
//�ú���Ӧ�ô���appkey���ҵ����ݿ��ж��ڵı�����������
void RWHandler::TransferDataToJson(string appkey)
{
	dataInJson.clear();

	sql::Statement *stmt;
	sql::ResultSet *res;

	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";
	//�����ݿ���л�ȡ������Ϣ
	string sqlStatement = "select * from " + appkey_tables[appkey] + " where fixed=\"false\"";
	res = stmt->executeQuery(sqlStatement);
	//ptree pt3, pt4;
	//ѭ������
	while (res->next())
	{
		//��ȡ������Ϣת��ΪJSON��ʽ
		std::stringstream stream;
		ptree pt, pt1, pt2;
		pt1.put("crash_id", res->getString("crash_id"));
		pt1.put("developer", res->getString("developer"));
		pt1.put("fixed", res->getString("fixed"));
		pt1.put("crash_type", res->getString("crash_type"));
		pt1.put("app_version", res->getString("app_version"));
		pt1.put("first_crash_date_time", res->getString("first_crash_date_time"));
		pt1.put("last_crash_date_time", res->getString("last_crash_date_time"));
		pt1.put("crash_context_digest", res->getString("crash_context_digest"));
		pt1.put("crash_context", res->getString("crash_context"));

		pt2.push_back(make_pair("", pt1));
		//pt3.push_back(make_pair("", pt1));
		pt.put_child("data", pt2);
		write_json(stream, pt);
		dataInJson.push_back(stream.str());
	}
	//����
	/*pt4.put_child("datas", pt3);
	std::stringstream stream;
	write_json(stream, pt4);
	writeFile(stream.str(), "dataJson.txt");*/

	//writeFile(dataInJson, "dataJson.txt");

	//����
	delete res;
	delete stmt;
}

//�ӿͻ����յ�������ϢJson��ʽ������JSON���������ݿ�
void RWHandler::UpdateDatabase(string clientData)
{
	string crash_id, developerId, fixed, appkey;

	ptree pt;
	std::stringstream stream;
	stream << clientData;
	read_json<ptree>(stream, pt);
	developerId = pt.get<string>("Developer");
	crash_id = pt.get<string>("CrashId");
	fixed = pt.get<string>("Solve");
	appkey = pt.get<string>("Appkey");

	//�������ݿ���Ϣ
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;
	//���쳣�����������
	if (developerId != "null")
	{
		string developer;
		sqlStatement = "select Name from developer where ID = \"" + developerId + "\"";
		pstmt = con->prepareStatement(sqlStatement);
		res = pstmt->executeQuery();
		while (res->next())
			developer = res->getString("Name");

		sql::Statement *stmt;
		stmt = con->createStatement();
		string sqlStateMent = "UPDATE " + appkey_tables[appkey] +" SET developer = \"" + developer
			+ "\",fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);

		delete stmt;
		delete pstmt;
		delete res;
		//���쳣��Ϣ�������·���
		AutoClassifyCrash(appkey_tables[appkey], developer);
	}
	//�쳣���Ϊ�ѽ��
	else
	{
		sql::Statement *stmt;
		stmt = con->createStatement();
		string sqlStateMent = "UPDATE " + appkey_tables[appkey] +" SET fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);
		delete stmt;
	}
}

//���¿����ߵ��쳣��Ϣʱ����Ҫ���¶��쳣���з���
void RWHandler::AutoClassifyCrash(string tableName, string developer)
{
	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_id, crash_context from " + tableName + " where developer = \"" + developer + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();

	//�쳣id�ļ���
	std::vector<string> crash_id;
	std::unordered_map<string, bool> hasClassy;
	//ÿ���쳣������Щģ��
	std::unordered_map<string, std::unordered_set<string>> crash_modules;
	//�ÿ����߸�����Щģ��
	std::unordered_set<string> crash_totalModules;

	while (res->next())
	{
		crash_id.push_back(res->getString("crash_id"));
		hasClassy[res->getString("crash_id")] = false;
		const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2,9}";
		boost::regex reg(regStr);
		boost::smatch m;
		string crash_context = res->getString("crash_context");
		while (boost::regex_search(crash_context, m, reg))
		{
			boost::smatch::iterator it = m.begin();
			crash_totalModules.insert(it->str());
			crash_modules[res->getString("crash_id")].insert(it->str());
			crash_context = m.suffix().str();
		}
	}

	delete pstmt;
	delete res;

	//��ʼ��ÿ���쳣��ģ������
	std::unordered_map<string, std::vector<int>> crash_vectors;
	std::unordered_set<string>::iterator iterTotalModules = crash_totalModules.begin();
	for (; iterTotalModules != crash_totalModules.end(); ++iterTotalModules)
	{
		std::unordered_map<string, std::unordered_set<string>>::iterator iter = crash_modules.begin();
		for (; iter != crash_modules.end(); ++iter)
		{
			if (iter->second.find((*iterTotalModules)) == iter->second.end())
				crash_vectors[iter->first].push_back(0);
			else
				crash_vectors[iter->first].push_back(1);
		}
	}
	//�쳣�����ͣ������쳣�������������ƶ��ж������Ƿ���ͬһ���쳣
	int type = 0;
	std::unordered_map<int, std::vector<string>> crash_types;
	std::unordered_map<string, std::vector<int>>::iterator iter = crash_vectors.begin();
	for (int i = 0; i < crash_id.size(); i++)
	{
		if (!hasClassy[crash_id[i]])
		{
			crash_types[type].push_back(crash_id[i]);
			if (crash_vectors.find(crash_id[i]) == crash_vectors.end())
				continue;
		}

		else
			continue;
		for (int j = i + 1; j < crash_id.size(); j++)
		{
			if (crash_vectors.find(crash_id[j]) == crash_vectors.end())
				continue;
			if (CalculateCos(crash_vectors[crash_id[i]], crash_vectors[crash_id[j]]))
			{
				crash_types[type].push_back(crash_id[j]);
				hasClassy[crash_id[j]] = true;
			}
		}
		++type;
	}

	//�����쳣�����ͣ��������������ݿ���
	con->setAutoCommit(0);
	for (int i = 0; i < type; i++)
	{
		for (int j = 0; j < crash_types[i].size(); j++)
		{
			std::stringstream ss;
			ss << i;
			sqlStatement = "update " + tableName + " set crash_type=\"" + ss.str() + "\" where crash_id = \"" + crash_types[i][j] + "\"";
			pstmt = con->prepareStatement(sqlStatement);
			pstmt->execute();
			delete pstmt;
		}
	}
	con->commit();
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