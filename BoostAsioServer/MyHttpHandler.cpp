/*从萌友平台获取数据，HTTP请求处理类,
同时把获取到的数据写入数据库中*/

#include<unordered_map>
#include <fstream>
#include <boost/asio.hpp>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>
#include <boost\date_time.hpp>
#include <boost\regex.hpp>

#include "MyHttpHandler.h"

#define BOOST_SPIRIT_THREADSAFE

using namespace boost::property_tree;
using namespace boost::gregorian;
using boost::asio::ip::tcp;

//UTF-8转GB2312，测试
char* U2G(const char* utf8)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	memset(wstr, 0, len + 1);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, len);
	len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* str = new char[len + 1];
	memset(str, 0, len + 1);
	WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, len, NULL, NULL);
	if (wstr) delete[] wstr;
	return str;
}

//GB2312转UTF-8，测试
char* G2U(const char* gb2312)
{
	int len = MultiByteToWideChar(CP_ACP, 0, gb2312, -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	memset(wstr, 0, len + 1);
	MultiByteToWideChar(CP_ACP, 0, gb2312, -1, wstr, len);
	len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* str = new char[len + 1];
	memset(str, 0, len + 1);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
	if (wstr) delete[] wstr;
	return str;
}

MyHttpHandler::MyHttpHandler()
{
	host = "myou.cvte.com";
	loginPage = "http://myou.cvte.com/api/in/auth/login";
	data = "{\"email\":\"lindexi@cvte.com\",\"password\":\"11111111\"}";
	origin = "http://myou.cvte.com";
	
	//连接数据库
	dirver = get_driver_instance();
	con = dirver->connect("localhost", "root", "123456");
	if (con == NULL)
	{
		std::cout << "不能连接数据库！" << std::endl;
		return;
	}
	//选择CrashKiller数据库
	con->setSchema("CrashKiller");
}
MyHttpHandler::MyHttpHandler(string a, string s, string e) : appkey(a), start_date(s), end_date(e)
{
	//设置Header信息
	host = "myou.cvte.com";
	loginPage = "http://myou.cvte.com/api/in/auth/login";
	data = "{\"email\":\"lindexi@cvte.com\",\"password\":\"11111111\"}";
	origin = "http://myou.cvte.com";

	//连接数据库
	dirver = get_driver_instance();
	con = dirver->connect("localhost", "root", "123456");
	if (con == NULL)
	{
		std::cout << "不能连接数据库！" << std::endl;
		return;
	}
	//选择CrashKiller数据库
	con->setSchema("CrashKiller");
}

void MyHttpHandler::setAppKey(string key)
{
	appkey = key;
}

void MyHttpHandler::setStartDate(string date)
{
	start_date = date;
}

void MyHttpHandler::setEndDate(string date)
{
	end_date = date;
}

void MyHttpHandler::PostHttpRequest()
{
	errorList = "";
	GetLoginToken();
	std::cout << "Get Token finish!" << std::endl;
	GetErrorList();
	std::cout << "Get Error List finish!" << std::endl;
}

//解析从萌友获取的数据，并插入数据库中，如果数据中有该异常的信息，则不再插入
//只插入新获取到的异常信息
void MyHttpHandler::ParseJsonAndInsertToDatabase()
{
	//sql::Statement *stmt;
	//stmt = con->createStatement();
	//stmt->execute("delete from errorinfo");
	//delete stmt;
	con->setAutoCommit(0);
	ptree pt, pt1, pt2;
	std::stringstream stream;
	stream << errorList;
	read_json<ptree>(stream, pt);

	pt1 = pt.get_child("data");

	for (ptree::iterator it = pt1.begin(); it != pt1.end(); ++it)
	{
		pt2 = it->second;
		//std::cout << "app_version:" << p1.get<string>("app_version") << std::endl;
		InsertDataToDataBase(pt2.get<string>("crash_id"), pt2.get<string>("fixed"),
			pt2.get<string>("app_version"), pt2.get<string>("first_crash_date_time"),
			pt2.get<string>("last_crash_date_time"), pt2.get<string>("crash_context_digest"), pt2.get<string>("crash_context"));

		//测试
		//InsertModulesInfo(pt2.get<string>("crash_context"));
		//InsertDeveloperInfo(p1.get<string>("crash_context_digest"));
	}

	con->commit();

	//int totalCount = pt.get<int>("totalCount");
	//std::cout << "totalCount:" << totalCount << std::endl;
	//p1 = pt.get_child("data");
	//for (ptree::iterator it = p1.begin(); it != p1.end(); ++it)
	//{
	//	p2 = it->second;
	//	std::cout << "context_digest:" << p2.get<string>("context_digest") << std::endl;
	//	std::cout << "raw_crash_record_id:" << p2.get<int>("raw_crash_record_id") << std::endl;
	//	std::cout << "created_at:" << p2.get<string>("created_at") << std::endl;
	//	std::cout << "app_version:" << p2.get<string>("app_version") << std::endl;
	//	/*InsertDataToDataBase(con, 
	//		i,
	//		p2.get<string>("context_digest"), 
	//		p2.get<string>("raw_crash_record_id"),
	//		p2.get<string>("created_at"),
	//		p2.get<string>("app_version"));*/
	//	i++;
	//}

	delete con;
}

//根据异常中堆栈的模块信息的权重智能分配异常，返回分配的开发者名字
string MyHttpHandler::AutoDistributeCrash(string crash_context)
{
	string developerName = "";
	string moduleName = "";
	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
	boost::regex reg(regStr);
	boost::smatch m;
	std::unordered_map<string, int> developerWeight;
	int maxWeight = 0;
	string maxId;

	//查找数据库信息
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;

	while (boost::regex_search(crash_context, m, reg))
	{
		boost::smatch::iterator it = m.begin();
		moduleName = it->str();


		sqlStatement = "select developer_id from moduleinfo where module_name = \"" + moduleName + "\"";
		//从errorinfo表中获取所有信息
		pstmt = con->prepareStatement(sqlStatement);
		res = pstmt->executeQuery();

		string developer_id;
		while (res->next())
			developer_id = res->getString("developer_id");

		delete pstmt;
		delete res;

		if (developerWeight.find(developer_id) == developerWeight.end())
			developerWeight[developer_id] = 1;
		else
		{
			//找出权重最大的开发者，因为一条异常中可能涉及几个模块，每个模块都由不同
			//开发者实现，一个模块给开发者的权重加1
			developerWeight[developer_id]++;
			if (developerWeight[developer_id] > maxWeight)
			{
				maxId = developer_id;
				maxWeight = developerWeight[developer_id];
			}
		}
		crash_context = m.suffix().str();
	}

	sqlStatement = "select Name from developer where ID = \"" + maxId + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	res = pstmt->executeQuery();

	while (res->next())
		developerName = res->getString("Name");

	delete pstmt;
	delete res;

	/*if (moduleName != "")
	{
		sql::PreparedStatement *pstmt;
		sql::ResultSet *res;

		string sqlStatement = "select developer_id from moduleinfo where module_name = \"" + moduleName + "\"";
		//从errorinfo表中获取所有信息
		pstmt = con->prepareStatement(sqlStatement);
		res = pstmt->executeQuery();
		
		string developer_id;
		while (res->next())
			developer_id = res->getString("developer_id");

		delete pstmt;
		delete res;

		sqlStatement = "";
		sqlStatement = "select Name from developer where ID = \"" + developer_id + "\"";
		con->prepareStatement(sqlStatement);
		res = pstmt->executeQuery();

		while(res->next())
			developerName = res->getString("Name");

		delete res;
	}*/
	
	return developerName;
}

//把同一个人的异常信息进行初步的分类
void MyHttpHandler::AutoClassifyCrash(string developer)
{
	/*sql::PreparedStatement *pstmt;
	string sqlStatement = "select developer from errorinfo group by developer";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();
	std::vector<string> developer;
	while (res->next())
	{
		if (res->getString("developer") != "")
			developer.push_back(res->getString("developer"));
	}

	delete pstmt;
	delete res;*/

	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_id, crash_context from errorinfo where developer = \"" + developer + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();
	std::unordered_map<string, string> crash;
	while (res->next())
		crash[res->getString("crash_id")] = res->getString("crash_context");



	delete pstmt;
	delete res;
}

//从异常堆栈信息中提取出模块信息,测试，初始化
void MyHttpHandler::InsertModulesInfo(string crash_context)
{
	//字符串匹配Cvte的信息
	/*int pos=crash_context.find("Cvte");
	while (pos != string::npos)
	{
		int i = pos;
		while (crash_context[i] != '(')
			i++;

		sql::PreparedStatement *pstmt;
		//插入模块信息，如果数据库中有相同主键的数据，则更新数据库信息
		pstmt = con->prepareStatement("INSERT IGNORE INTO moduleInfo(module_name) VALUES(?)");
		pstmt->setString(1, crash_context.substr(pos,i-pos));
		pstmt->execute();
		delete pstmt;

		pos = crash_context.find("Cvte", i + 1);
	}*/

	//正则表达式匹配固定格式的字符串
	//const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){1,4}";
	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
	boost::regex reg(regStr);
	boost::smatch m;
	while (boost::regex_search(crash_context, m, reg))
	{
		//遍历模块信息
		/*for (boost::smatch::iterator it = m.begin(); it != m.end(); it+=2)
		{
			std::cout << it->str() << std::endl;
			sql::PreparedStatement *pstmt;
			//插入模块信息，如果数据库中有相同主键的数据，则更新数据库信息
			pstmt = con->prepareStatement("INSERT IGNORE INTO moduleInfo(module_name) VALUES(?)");
			pstmt->setString(1, it->str());
			pstmt->execute();
			delete pstmt;
		}*/

		boost::smatch::iterator it = m.begin();
		//std::cout << it->str() << std::endl;
		sql::PreparedStatement *pstmt;
		//插入模块信息，如果数据库中有相同主键的数据，则更新数据库信息
		pstmt = con->prepareStatement("INSERT IGNORE INTO moduleInfo(module_name) VALUES(?)");
		pstmt->setString(1, it->str());
		pstmt->execute();
		delete pstmt;

		crash_context = m.suffix().str();
	}
}

void MyHttpHandler::InsertDataToDataBase(const string crash_id, const string fixed, const string app_version
	, const string first_crash_date_time, const string last_crash_date_time, const string crash_context_digest,
	const string crash_context)
{
	////从异常信息中提取出开发者的id
	////第一次出现等号的位置
	//int equalIndex = crash_context_digest.find_first_of("=");
	////第一次出现左括号的位置
	//int leftIndex = crash_context_digest.find_first_of("(");
	////开发者id的长度
	//int idLength = leftIndex - 1 - equalIndex;
	//string id;
	//if (idLength > 0)
	//	id = crash_context_digest.substr(equalIndex + 1, idLength);
	//else
	//	id = "";

	//首先判断数据库中是否已存在该异常数据
	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_context from errorinfo where crash_id = \"" + crash_id + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();
	bool isExisted = false;
	while (res->next())
		isExisted = true;

	delete pstmt;
	delete res;
	//不存在则添加到数据库中
	if (!isExisted)
	{
		//根据异常信息中的模块信息把异常智能分配给开发者
		string developerName = AutoDistributeCrash(crash_context);

		//使用replace语句，如果数据库中有相同主键的数据，则更新数据库信息
		//pstmt = con->prepareStatement("REPLACE INTO ErrorInfo(crash_id,developerid,fixed,app_version,first_crash_date_time,last_crash_date_time,crash_context_digest,crash_context) VALUES(?,?,?,?,?,?,?,?)");
		pstmt = con->prepareStatement("INSERT INTO ErrorInfo(crash_id,developer,fixed,app_version,first_crash_date_time,last_crash_date_time,crash_context_digest,crash_context) VALUES(?,?,?,?,?,?,?,?)");

		pstmt->setString(1, crash_id);
		pstmt->setString(2, developerName);
		pstmt->setString(3, fixed);
		pstmt->setString(4, app_version);
		pstmt->setString(5, first_crash_date_time);
		pstmt->setString(6, last_crash_date_time);
		pstmt->setString(7, crash_context_digest);
		pstmt->setString(8, crash_context);

		pstmt->execute();

		delete pstmt;
	}
}

int MyHttpHandler::GetErrorList()
{
	analyzePage = "http://myou.cvte.com/api/in/analyze/" + appkey
		+ "/crash_list?platform=windows_app&app_version=&start_date=" + start_date
		+ "&end_date=" + end_date
		+ "&limit=50&offset=0&fixed=false";
	try
	{
		boost::asio::io_service io_service;
		//如果io_service存在复用的情况
		if (io_service.stopped())
			io_service.reset();

		// 从dns取得域名下的所有ip
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(host, "http");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

		// 尝试连接到其中的某个ip直到成功 
		tcp::socket socket(io_service);
		boost::asio::connect(socket, endpoint_iterator);

		// Form the request. We specify the "Connection: close" header so that the 
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		boost::asio::streambuf request;
		std::ostream request_stream(&request);
		request_stream << "GET " << analyzePage << " HTTP/1.0\r\n";
		request_stream << "Host: " << host << "\r\n";
		request_stream << "Authorization:" << token << "\r\n";
		request_stream << "Accept:application/json,text/plain, */*\r\n";
		request_stream << "User_Agent:" << "Mozilla/5.0(Windows NT 10.0;WOW64) AppleWebKit/537.36(KHTML,like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n";
		request_stream << "Content-Type: application/json;charset=UTF-8\r\n";
		request_stream << "Accept-Encoding:gzip,deflate\r\n";
		request_stream << "Accept-Language:zh-CN,zh;q=0.8\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Send the request.
		boost::asio::write(socket, request);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		boost::asio::streambuf response;
		boost::asio::read_until(socket, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			errorList = "";
			std::cout << "Invalid response" << std::endl;
			return -2;
		}
		// 如果服务器返回非200都认为有错,不支持301/302等跳转
		if (status_code != 200)
		{
			errorList = "";
			std::cout << "Response returned with status code != 200 " << std::endl;
			return status_code;
		}

		// 传说中的包头可以读下来了
		std::string header;
		std::vector<string> headers;
		while (std::getline(response_stream, header) && header != "\r")
			headers.push_back(header);

		// 读取所有剩下的数据作为包体
		//std::string dataListInfo;
		boost::system::error_code error;
		while (boost::asio::read(socket, response,
			boost::asio::transfer_at_least(1), error))
		{
			std::istream is(&response);
			is.unsetf(std::ios_base::skipws);
			errorList.append(std::istream_iterator<char>(is), std::istream_iterator<char>());
			//std::cout << &response;
		}
		//把utf8格式转换为GB2312
		//char *myData;
		//myData = U2G(errorList.c_str());
		/*for (int i = 0; i < strlen(myData); i++)
		{
		std::cout << myData[i];
		}
		std::cout << std::endl;*/
		//把数据写入到文件中
		writeFile(errorList.c_str(), "receiveData.txt");
	}
	catch (std::exception& e)
	{
		errorList = "";
		std::cout << e.what() << std::endl;
		return -4;
	}
	return 0;
}

int MyHttpHandler::GetLoginToken()
{
	try
	{
		boost::asio::io_service io_service;
		//如果io_service存在复用的情况
		if (io_service.stopped())
			io_service.reset();

		// 从dns取得域名下的所有ip
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(host, "http");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

		// 尝试连接到其中的某个ip直到成功 
		tcp::socket socket(io_service);
		boost::asio::connect(socket, endpoint_iterator);

		// Form the request. We specify the "Connection: close" header so that the 
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		boost::asio::streambuf request;
		std::ostream request_stream(&request);
		request_stream << "POST " << loginPage << " HTTP/1.0\r\n";
		request_stream << "Host: " << host << "\r\n";
		request_stream << "Connection: keep-alive\r\n";
		request_stream << "Content-Length: " << data.length() << "\r\n";
		request_stream << "Accept:application/json,text/plain, */*\r\n";
		request_stream << "Origin:" << origin << "\r\n";
		request_stream << "User_Agent:" << "Mozilla/5.0(Windows NT 10.0;WOW64) AppleWebKit/537.36(KHTML,like Gecko) Chrome/51.0.2704.106 Safari/537.36\r\n";
		request_stream << "Content-Type: application/json;charset=UTF-8\r\n";
		request_stream << "Referer:" << origin << "/" << "\r\n";
		request_stream << "Accept-Encoding:gzip,deflate\r\n";
		request_stream << "Accept-Language:zh-CN,zh;q=0.8\r\n\r\n";
		request_stream << data;

		// Send the request.
		boost::asio::write(socket, request);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		boost::asio::streambuf response;
		boost::asio::read_until(socket, response, "\r\n");

		// Check that response is OK.
		std::istream response_stream(&response);
		std::string http_version;
		response_stream >> http_version;
		unsigned int status_code;
		response_stream >> status_code;
		std::string status_message;
		std::getline(response_stream, status_message);
		if (!response_stream || http_version.substr(0, 5) != "HTTP/")
		{
			token = "";
			std::cout << "Invalid response" << std::endl;
			return -2;
		}
		// 如果服务器返回非200都认为有错,不支持301/302等跳转
		if (status_code != 200)
		{
			token = "";
			std::cout << "Response returned with status code != 200 " << std::endl;
			return status_code;
		}

		// 传说中的包头可以读下来了
		std::string header;
		std::vector<string> headers;
		while (std::getline(response_stream, header) && header != "\r")
			headers.push_back(header);

		// 读取所有剩下的数据作为包体
		std::string tokenInfo;
		boost::system::error_code error;
		boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error);
		std::istream is(&response);
		is.unsetf(std::ios_base::skipws);
		tokenInfo.append(std::istream_iterator<char>(is), std::istream_iterator<char>());
		//writeFile(tokenInfo.c_str(), "token.txt");

		ptree pt, pt1;
		std::stringstream stream;
		stream << tokenInfo;
		read_json<ptree>(stream, pt);
		pt1 = pt.get_child("data");
		token = pt1.get<string>("token");

		/*if (error != boost::asio::error::eof)
		{
		token = "";
		std::cout << error.message() << std::endl;
		return -3;
		}*/
	}
	catch (std::exception& e)
	{
		token = "";
		std::cout << e.what() << std::endl;
		return -4;
	}
	return 0;
}

void MyHttpHandler::writeFile(const char *src, const char *fileName)
{
	std::ofstream out(fileName, std::ios::out);
	if (!out)
	{
		std::cout << "Can not Open this file!" << std::endl;
		return;
	}
	out << src;
	out << std::endl;

	out.close();
}

//根据异常信息提取开发者信息
void MyHttpHandler::InsertDeveloperInfo(string info)
{
	//第一次出现等号的位置
	int equalIndex = info.find_first_of("=");
	//第一次出现左括号的位置
	int leftIndex = info.find_first_of("(");
	//第一次出现右括号的位置
	int rightIndex = info.find_first_of(")");
	//开发者id的长度
	int idLength = leftIndex - 1 - equalIndex;
	string id;
	if (idLength > 0)
		id = info.substr(equalIndex + 1, idLength);
	else
		return;
	//开发者名字的长度
	string name;
	int nameLeghth = rightIndex - 1 - leftIndex;
	if (nameLeghth > 0)
		name = info.substr(leftIndex + 1, nameLeghth);
	else
		return;

	sql::PreparedStatement *pstmt;
	pstmt = con->prepareStatement("INSERT IGNORE INTO developer(ID,Name) VALUES(?,?)");

	pstmt->setString(1, id);
	pstmt->setString(2, name);

	pstmt->execute();

	delete pstmt;
}