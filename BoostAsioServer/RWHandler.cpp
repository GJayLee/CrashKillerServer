/*
处理服务端与客户端进行读写操作
*/

#include "RWHandler.h"

const int SEND_SIZE = 65535;

const string STX = "\x2";       //正文开始
const string EOT = "\x4";       //正文结束
const string ETX = "\x3";       //收到

RWHandler::RWHandler(io_service& ios) : m_sock(ios), sendIds(9)
{
	//httphandler = new MyHttpHandler();
	offSet = 0;
	dataToSendIndex = 0;
	initErrorInfo = false;
	initDeveloper = false;
	/*appKey = "";
	start_date = "";
	end_date = "";*/

	int current = 0;
	std::generate_n(sendIds.begin(), 9, [&current] {return ++current; });

	dirver = get_driver_instance();
	//连接数据库
	con = dirver->connect("localhost", "root", "123456");
	//选择mydata数据库
	con->setSchema("CrashKiller");

	InitProjectsTables();
}

RWHandler::~RWHandler()
{
	delete con;
}

void RWHandler::HandleRead()
{
	//三种情况下会返回：1.缓冲区满；2.transfer_at_least为真(收到特定数量字节即返回)；3.有错误发生
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
	/*boost::system::error_code ec;
	write(m_sock, buffer(data, len), ec);
	if (ec != nullptr)
	HandleError(ec);*/
	// 发送信息(非阻塞)
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

		//没有超过最大缓冲区，可以完整发送
		if (dataInJson[dataToSendIndex].size() <= SEND_SIZE)
		{
			//添加结束符告诉客户端这是该data的最后一段
			string sendStr = GetSendData(EOT, dataInJson[dataToSendIndex]);
			m_sock.async_write_some(buffer(sendStr.c_str(), sendStr.size()),
				boost::bind(&RWHandler::write_handler, this, placeholders::error));
		}
		else
		{
			/*string endStr = "End" + dataInJson[dataToSendIndex];
			m_sock.async_write_some(buffer(endStr.c_str() + offSet, dataInJson[dataToSendIndex].size() - offSet + 3),
			boost::bind(&RWHandler::write_handler, this, placeholders::error));*/
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

		/*int sendSize = 0;
		if (strlen(sendData) - offSet < SEND_SIZE)
		sendSize = strlen(sendData) - offSet;
		else
		sendSize = SEND_SIZE;
		m_sock.async_write_some(buffer(sendData + offSet, sendSize),
		boost::bind(&RWHandler::write_handler, this, placeholders::error));*/
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

//string RWHandler::GetAppKey()
//{
//	//return appKey;
//}
//
//string RWHandler::GetStartDate()
//{
//	//return start_date;
//}
//
//string RWHandler::GetEndDate()
//{
//	//return end_date;
//}

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

	ptree pt, pt1, pt2;
	read_json<ptree>(buffer, pt);
	pt1 = pt.get_child("projects");
	for (ptree::iterator it = pt1.begin(); it != pt1.end(); ++it)
	{
		pt2 = it->second;
		tables.push_back(pt2.get<string>("tableName"));
	}
}

/*void setSendData(const char* str)
{
sendData = new char[strlen(str) + 1];
memset(sendData, 0, sizeof(char)*(strlen(str) + 1));
strcpy(sendData, str);
offSet = 0;
}*/

// 异步写操作完成后write_handler触发
//void write_handler(const boost::system::error_code& ec, boost::shared_ptr<std::string> str)
void RWHandler::write_handler(const boost::system::error_code& ec)
{
	if (ec)
	{
		std::cout << "发送失败!" << std::endl;
		HandleError(ec);
	}
	else
	{
		//std::cout << "成功发送！" << std::endl;
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
		//客户端请求初始化异常数据时调用
		//if (initErrorInfo || strcmp(&(*str)[0], "Get Expect") == 0)
		if (initErrorInfo || strcmp(&(*str)[2], "Get Expect") == 0)
		{
			//HandleWrite();
			if (!initErrorInfo)
			{
				//GetDatabaseData();
				boost::system::error_code ec;
				string sendStr = ETX + (*str)[1] + "Receive";
				write(m_sock, buffer(sendStr, sendStr.size()), ec);

				Sleep(500);

				initErrorInfo = true;
				TransferDataToJson();
				dataToSendIndex = 0;
				offSet = 0;
				HandleWrite();
			}
			else
			{
				if ((*str)[0] == ETX[0])
				{
					int sendId = (*str)[1] - '0';
					RecyclSendId(sendId);
					if ((offSet + SEND_SIZE) > dataInJson[dataToSendIndex].size())
					{
						//std::cout << (*str)[1] << "发送完成！" << std::endl;
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
							string sendStr = GetSendData(EOT,"SendFinish");
							write(m_sock, buffer(sendStr, sendStr.size()), ec);
							initErrorInfo = false;

							HandleRead();
							////关闭连接
							//CloseSocket();
							//std::cout << "断开连接" << m_connId << std::endl;
							//if (m_callbackError)
							//	m_callbackError(m_connId);
						}
					}
					else
					{
						//std::cout << "接收消息：" << &(*str)[0] << std::endl;
						offSet = offSet + SEND_SIZE;
						HandleWrite();
					}
				}
				else
				{
					std::cout << "没有接收到返回，重发消息!" << std::endl;
					HandleWrite();
				}
			}
		}
		//客户端发送获取开发者信息时
		else if (initDeveloper || strcmp(&(*str)[2], "Get Develop") == 0)
		{
			if (!initDeveloper)
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
			else
			{
				if ((*str)[0] == ETX[0])
				{
					int sendId = (*str)[1] - '0';
					RecyclSendId(sendId);
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
				else
				{
					std::cout << "没有接收到返回，重发消息!" << std::endl;
					HandleWrite();
				}
			}
		}
		//客户端返回更新内容，如分配给哪个用户，异常是否已解决时调用
		//else if (strcmp(&(*str)[2], "Update") == 0)
		//{
		//	//UpdateDatabase("0533e5d3-7bf6-4a75-95c9-b5b3b3264880", "18520147781", "false");
		//	HandleRead();
		//}
		//其他情况，测试
		else
		{
			std::cout << "接收消息：" << &(*str)[0] << std::endl;
			//从客户端收到数据更新数据库
			char command[3] = { 0 };
			for (int i = 2; i < 4; i++)
				command[i-2] = (*str)[i];
			if (strcmp(command, "Up") == 0)
			{
				UpdateDatabase(&(*str)[4]);
				boost::system::error_code ec;
				string sendStr = ETX + (*str)[1] + "UpdateFinish";
				write(m_sock, buffer(sendStr, sendStr.size()), ec);
				HandleRead();
			}
			else
				HandleRead();
			
			//char command[7] = { 0 };
			//char msg[100] = { 0 };
			//for (int i = 0; i < 6; i++)
			//{
			//	command[i] = (*str)[i];
			//	msg[i] = (*str)[i];
			//}
			//for (int i = 6; i < str->size(); i++)
			//	msg[i] = (*str)[i];
			//if (strcmp(command, "Update") == 0)
			//{
			//	//std::cout << "接收消息：" << command << std::endl;
			//	std::vector<std::string> vec;
			//	boost::split(vec, msg, boost::is_any_of("+"));
			//	appKey = vec[1];
			//	start_date = vec[2];
			//	end_date = vec[3];
			//	httphandler->setAppKey(appKey);
			//	httphandler->setStartDate(start_date);
			//	httphandler->setEndDate(end_date);
			//	httphandler->PostHttpRequest();
			//	//setSendData(errorList.c_str());
			//	httphandler->ParseJsonAndInsertToDatabase();
			//	offSet = 0;
			//	initErrorInfo = true;
			//	HandleWrite();
			//}
		}
	}
}

//根据传输协议拼接发送消息     head+count+data
string RWHandler::GetSendData(string flag, string msg)
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
	std::cout << "断开连接" << m_connId << std::endl;
	//std::cout << ec.message() << std::endl;
	if (m_callbackError)
		m_callbackError(m_connId);
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
void RWHandler::TransferDataToJson()
{
	dataInJson.clear();

	sql::Statement *stmt;
	sql::ResultSet *res;

	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";
	//从数据库表中获取所有信息
	string sqlStatement = "select * from " + tables[0] + " where fixed=\"false\"";
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
//void RWHandler::UpdateDatabase(string crash_id, string developer, string fixed)
{
	string crash_id, developerId, fixed;

	ptree pt;
	std::stringstream stream;
	stream << clientData;
	read_json<ptree>(stream, pt);
	developerId = pt.get<string>("Developer");
	crash_id = pt.get<string>("CrashId");
	fixed = pt.get<string>("Solve");

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
		string sqlStateMent = "UPDATE errorinfo SET developer = \"" + developer
			+ "\",fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);

		delete stmt;
		delete pstmt;
		delete res;
	}
	//异常标记为已解决
	else
	{
		sql::Statement *stmt;
		stmt = con->createStatement();
		string sqlStateMent = "UPDATE errorinfo SET fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);
		delete stmt;
	}
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

//把数据写入到文件方便测试
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

//把数据写入到文件方便测试
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