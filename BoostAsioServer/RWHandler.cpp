/*
处理服务端与客户端进行读写操作
*/

#include <unordered_set>
#include <boost\regex.hpp>

#include "RWHandler.h"
#include "generic.h"

const int SEND_SIZE = 65535;

const string STX = "\x2";       //正文开始
const string EOT = "\x4";       //正文结束
const string ETX = "\x3";       //收到

const char* INIT_CRASH_INFO = "Get Expect";           //初始化异常信息的请求命令
const char* INIT_DEVELOPER_INFO = "Get Develop";      //初始化开发者信息的请求命令
const char* UPDATE_CRASH_INFO = "Up";                 //请求更新的异常信息的命令

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
	//连接数据库
	con = dirver->connect("localhost", "root", "123456");
	//选择mydata数据库
	con->setSchema("CrashKiller");

	InitProjectsTables();

	//开启异步等待
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
		//没有超过最大缓冲区，可以完整发送
		if (dataInJson[dataToSendIndex].size() <= SEND_SIZE)
		{
			std::cout << "发送" << dataToSendIndex << std::endl;
			//添加结束符告诉客户端这是该data的最后一段
			string sendStr = GetSendData(EOT, dataInJson[dataToSendIndex]);
			m_sock.async_write_some(buffer(sendStr.c_str(), sendStr.size()),
				boost::bind(&RWHandler::write_handler, this, placeholders::error));
		}
		else
		{
			//把数据截断发送
			//添加结束符告诉客户端这是该data的最后一段
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
		//从发送起计算，10s后没接收到返回就重发
		isReSend = true;
		m_timer.expires_from_now(boost::posix_time::seconds(10));
	}
	//发送开发者信息
	if (initDeveloper)
	{
		//没有超过最大缓冲区，可以完整发送
		if (developerInfo.size() <= SEND_SIZE)
		{
			//添加结束符告诉客户端这是该data的最后一段
			string sendStr = GetSendData(EOT, developerInfo);
			m_sock.async_write_some(buffer(sendStr.c_str(), sendStr.size()),
				boost::bind(&RWHandler::write_handler, this, placeholders::error));
		}
		else
		{
			//把数据截断发送
			//添加结束符告诉客户端这是该data的最后一段
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
		std::cout << "发送失败!" << std::endl;
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
		//std::cout << "没有接收到消息！" << std::endl;
		HandleError(ec);
		return;
	}
	else
	{
		//从接收到的信息中获取请求命令
		char command[30] = { 0 };
		for (int i = 2; i < 30; i++)
		{
			if ((*str)[i] == '\0')
				break;
			command[i - 2] = (*str)[i];
		}
		//客户端请求初始化异常数据时调用
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
		//客户端发送获取开发者信息时
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
		//收到确认返回时，根据返回执行不同的操作
		else if ((*str)[0] == ETX[0])
		{
			//收到返回，设置重发为false
			isReSend = false;
			if (initErrorInfo)
			{
				int sendId = (*str)[1] - '0';
				//RecyclSendId(sendId);
				if ((offSet + SEND_SIZE) > dataInJson[dataToSendIndex].size())
				{
					std::cout << dataToSendIndex << "发送完成！" << std::endl;
					++dataToSendIndex;
					//判断是否还有下一条异常，如果有则继续发送，无则关闭连接
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
					std::cout << "开发者信息发送完成！" << std::endl;
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
		//客户端返回更新内容，如分配给哪个用户，异常是否已解决时调用
		else if(strcmp(command, UPDATE_CRASH_INFO) == 0)
		{
			std::cout << "接收消息：" << &(*str)[0] << std::endl;
			UpdateDatabase(&(*str)[4]);
			boost::system::error_code ec;
			string sendStr = ETX + (*str)[1] + "UpdateFinish";
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

//根据传输协议拼接发送消息     head+count+data
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
	std::cout << "断开连接" << m_connId << std::endl;
	//std::cout << ec.message() << std::endl;
	if (m_callbackError)
		m_callbackError(m_connId);
}

//等待10s，如果没收到返回，即isReSend为true则调用重发
void RWHandler::wait_handler()
{
	if (isDisConnected)
		return;

	if (m_timer.expires_at() <= deadline_timer::traits_type::now()&&isReSend)
	{
		std::cout << "没有接收到返回，重发消息!" << std::endl;
		HandleWrite();
	}
	m_timer.async_wait(boost::bind(&RWHandler::wait_handler, this));
}

//从项目配置文件中获取appkey
void RWHandler::InitProjectsTables()
{
	//读取配置文件中appkey信息
	std::ifstream t("ProjectsConfigure.txt");
	if (!t)
	{
		std::cout << "打开项目Appkey配置文件失败！" << std::endl;
		return;
	}
	std::stringstream buffer;
	buffer << t.rdbuf();
	//初始化项目配置信息
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

//从数据库中获取开发者信息并转换为JSON格式
void RWHandler::GetDeveloperInfo()
{
	sql::Statement *stmt;
	sql::ResultSet *res;

	stmt = con->createStatement();
	developerInfo = "";
	//从developer表中获取所有信息
	res = stmt->executeQuery("select * from developer");
	std::stringstream stream;
	ptree pt, pt2;
	//循环遍历
	while (res->next())
	{
		ptree pt1;
		//把取出的信息转换为JSON格式
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

//从数据库中获取数据并把数据转为JSON格式上, 此处目前因为只有一个appkey
//该函数应该传入appkey来找到数据库中对于的表，现在有问题
void RWHandler::TransferDataToJson(string appkey)
{
	dataInJson.clear();

	sql::Statement *stmt;
	sql::ResultSet *res;

	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";
	//从数据库表中获取所有信息
	string sqlStatement = "select * from " + appkey_tables[appkey] + " where fixed=\"false\"";
	res = stmt->executeQuery(sqlStatement);
	//ptree pt3, pt4;
	//循环遍历
	while (res->next())
	{
		//把取出的信息转换为JSON格式
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
	//测试
	/*pt4.put_child("datas", pt3);
	std::stringstream stream;
	write_json(stream, pt4);
	writeFile(stream.str(), "dataJson.txt");*/

	//writeFile(dataInJson, "dataJson.txt");

	//清理
	delete res;
	delete stmt;
}

//从客户端收到更新信息Json格式，解析JSON，更新数据库
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

	//查找数据库信息
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;
	//把异常分配给开发者
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
		//对异常信息进行重新分类
		AutoClassifyCrash(appkey_tables[appkey], developer);
	}
	//异常标记为已解决
	else
	{
		sql::Statement *stmt;
		stmt = con->createStatement();
		string sqlStateMent = "UPDATE " + appkey_tables[appkey] +" SET fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);
		delete stmt;
	}
}

//更新开发者的异常信息时，需要重新对异常进行分类
void RWHandler::AutoClassifyCrash(string tableName, string developer)
{
	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_id, crash_context from " + tableName + " where developer = \"" + developer + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();

	//异常id的集合
	std::vector<string> crash_id;
	std::unordered_map<string, bool> hasClassy;
	//每个异常中有哪些模块
	std::unordered_map<string, std::unordered_set<string>> crash_modules;
	//该开发者负责哪些模块
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

	//初始化每个异常的模块向量
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
	//异常的类型，根据异常向量的余弦相似度判断它们是否是同一个异常
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

	//遍历异常的类型，并把它插入数据库中
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

//从数据库中取出数据，未处理，测试
void RWHandler::GetDatabaseData()
{
	sql::Driver *dirver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;
	dirver = get_driver_instance();
	//连接数据库
	con = dirver->connect("localhost", "root", "123456");
	//选择mydata数据库
	con->setSchema("CrashKiller");
	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";

	//从name_table表中获取所有信息
	res = stmt->executeQuery("select * from errorinfo");
	//循环遍历
	while (res->next())
	{
		//输出，id，name，age,work,others字段的信息
		//cout << res->getString("name") << " | " << res->getInt("age") << endl;
		result = result + res->getString("ID") + res->getString("crash_id")
			+ res->getString("fixed") + res->getString("app_version")
			+ res->getString("first_crash_date_time") + res->getString("crash_context_digest")
			+ res->getString("crash_context");
	}

	//setSendData(result.c_str());

	//清理
	delete res;
	delete stmt;
	delete con;
}