/*从萌友平台获取数据，HTTP请求处理类,
同时把获取到的数据写入数据库中*/

#include<unordered_map>
#include <fstream>
#include <boost/asio.hpp>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>
#include <boost\date_time.hpp>
#include <boost\regex.hpp>
#include<algorithm>
#include<math.h>

#include "MyHttpHandler.h"

#define BOOST_SPIRIT_THREADSAFE

using namespace boost::property_tree;
using namespace boost::gregorian;
using boost::asio::ip::tcp;

//每次请求从萌友获取数据的最大数量,字符型用来拼接HTTP请求，整型用来判断是否达到最大值
const string LIMIT_NUM = "50";
const int MAX_LIMIT = 50;

//从萌友平台获取异常数据时的默认appKey、start_date和end_date
const string defaultAppKey = "9e4b010a0d51f6e020ead6ce37bad33896a00f90";
const string defaultStartDate = "2016-01-1";
const string defaultEndDate = "2016-07-26";

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

MyHttpHandler::~MyHttpHandler()
{
	delete con;
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

//解析从萌友获取的数据，并插入数据库中，如果数据中有该异常的信息，则不再插入
//只插入新获取到的异常信息
void MyHttpHandler::ParseJsonAndInsertToDatabase()
{
	//sql::Statement *stmt;
	//stmt = con->createStatement();
	//stmt->execute("delete from errorinfo");
	//delete stmt;
	
	//统计该请求是否已达到最大值，limit
	count = 0;
	//如果获取的异常数列为空，则证明已接收完成
	if (errorList == "")
		return;

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

		count++;
		//测试
		//InsertModulesInfo(pt2.get<string>("crash_context"));
		//InsertDeveloperInfo(p1.get<string>("crash_context_digest"));
	}

	con->commit();
}

//向萌友请求数据，并写入数据库
void MyHttpHandler::excuteAction()
{
	setAppKey(defaultAppKey);
	setStartDate(defaultStartDate);
	setEndDate(defaultEndDate);
	//PostHttpRequest();
	
	//第一次向萌友请求数据，最大数量为50个
	errorList = "";
	GetLoginToken();
	std::cout << "Get Token finish!" << std::endl;
	//第一次请求偏移量为0，从第一个位置开始获取
	GetErrorList("0");
	//解析第一次获取的数据，并写入数据库中
	ParseJsonAndInsertToDatabase();

	//计算偏移量
	int offset = 1;
	//如果接收达到最大限制，则修改offset，继续请求查看是否还有别的数据
	while (count >= MAX_LIMIT)
	{
		errorList = "";
		std::stringstream offsetStr;
		int offsetIndex = MAX_LIMIT*offset;
		offsetStr << offsetIndex;
		//计算偏移量，从偏移量处开始获取
		GetErrorList(offsetStr.str());
		ParseJsonAndInsertToDatabase();
		++offset;
	}
	std::cout << "Get Error List finish!" << std::endl;
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

//把同一个人的异常信息进行初步的分类,未完成
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

	std::vector<string> crash_id;
	std::vector<string> crash_context;
	std::unordered_map<int, std::vector<int>> crash_sim;

	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_id, crash_context from errorinfo where developer = \"" + developer + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();

	while (res->next())
	{
		crash_id.push_back(res->getString("crash_id"));
		crash_context.push_back(res->getString("crash_context"));
	}
	for (int i = 0; i < crash_context.size()-1; i++)
	{
		crash_sim[i].push_back(0);
		for (int j = i+1; j < crash_context.size(); j++)
		{
			int similarity = Levenshtein(crash_context[i], crash_context[j]);
			crash_sim[i].push_back(similarity);
			crash_sim[j].push_back(similarity);
		}
	}

	delete pstmt;
	delete res;
}

//简单字符串对比，返回相似度
int MyHttpHandler::Levenshtein(string str1, string str2)
{
	int len1 = str1.length();
	int len2 = str2.length();
	int **dif = new int*[len1 + 1];
	for (int i = 0; i < len1 + 1; i++)
	{
		dif[i] = new int[len2 + 1];
		memset(dif[i], 0, sizeof(int)*(len2 + 1));
	}
	for (int i = 0; i <= len1; i++)
		dif[i][0] = i;
	for (int i = 0; i <= len2; i++)
		dif[0][i] = i;

	for (int i = 1; i <= len1; i++)
	{
		for (int j = 1; j <= len2; j++)
		{
			if (str1[i - 1] == str2[j - 1])
				dif[i][j] = dif[i - 1][j - 1];
			else
				dif[i][j] = min(dif[i][j - 1], dif[i - 1][j], dif[i - 1][j - 1]) + 1;
		}
	}
	float similarity = 1 - (float)dif[len1][len2] / std::max(len1, len2);
	//百分比
	similarity *= 100;

	for (int i = 0; i < len1 + 1; i++)
		delete[] dif[i];
	
	return (int)similarity;
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

int MyHttpHandler::GetErrorList(string offset)
{
	analyzePage = "http://myou.cvte.com/api/in/analyze/" + appkey
		+ "/crash_list?platform=windows_app&app_version=&start_date=" + start_date
		+ "&end_date=" + end_date
		+ "&limit=" + LIMIT_NUM + "&offset=" + offset + "&fixed=false";
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

//求三个数的最小值
int MyHttpHandler::min(int a, int b, int c) {
	if (a > b) {
		if (b > c)
			return c;
		else
			return b;
	}
	if (a > c) {
		if (c > b)
			return b;
		else
			return c;
	}
	if (b > c) {
		if (c > a)
			return a;
		else
			return c;
	}
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