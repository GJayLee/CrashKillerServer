#pragma once
#include <fstream>
#include <boost/asio.hpp>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>
#include <boost\date_time.hpp>

//连接数据库
#include<cppconn\driver.h>
#include<cppconn\exception.h>
#include <cppconn/resultset.h> 
#include <cppconn/statement.h>
#include<mysql_connection.h>
#include <cppconn/prepared_statement.h>

#define BOOST_SPIRIT_THREADSAFE

using namespace boost::property_tree;
using namespace boost::gregorian;
using boost::asio::ip::tcp;
using std::string;

//UTF-8转GB2312
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

//GB2312转UTF-8
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

/*从萌友平台获取数据，HTTP请求处理类,
同时把获取到的数据写入数据库中*/
class MyHttpHandler
{
public:
	MyHttpHandler()
	{
		host = "myou.cvte.com";
		loginPage = "http://myou.cvte.com/api/in/auth/login";
		data = "{\"email\":\"lindexi@cvte.com\",\"password\":\"11111111\"}";
		origin = "http://myou.cvte.com";
	}
	MyHttpHandler(string a, string s, string e) : appkey(a), start_date(s), end_date(e)
	{
		//设置Header信息
		host = "myou.cvte.com";
		loginPage = "http://myou.cvte.com/api/in/auth/login";
		data = "{\"email\":\"lindexi@cvte.com\",\"password\":\"11111111\"}";
		origin = "http://myou.cvte.com";
	}

	void setAppKey(string key)
	{
		appkey = key;
	}

	void setStartDate(string date)
	{
		start_date = date;
	}

	void setEndDate(string date)
	{
		end_date = date;
	}

	string PostHttpRequest()
	{
		errorList = "";
		GetLoginToken();
		std::cout << "Get Token finish!" << std::endl;
		GetErrorList();
		std::cout << "Get Error List finish!" << std::endl;

		return errorList;
	}

	void ParseJsonAndInsertToDatabase()
	{
		//连接数据库
		sql::Driver *dirver;
		sql::Connection *con;
		dirver = get_driver_instance();
		//连接数据库
		con = dirver->connect("localhost", "root", "123456");
		//选择CrashKiller数据库
		con->setSchema("CrashKiller");
		
		sql::Statement *stmt;
		stmt = con->createStatement();
		stmt->execute("delete from errorinfo");
		delete stmt;
		
		ptree pt, pt1, pt2;
		std::stringstream stream;
		stream << errorList;
		read_json<ptree>(stream, pt);
		
		pt1 = pt.get_child("data");
		
		for (ptree::iterator it = pt1.begin(); it != pt1.end(); ++it)
		{
			pt2 = it->second;
			//std::cout << "app_version:" << p1.get<string>("app_version") << std::endl;
			InsertDataToDataBase(con, pt2.get<string>("crash_id"), pt2.get<string>("fixed"),
				pt2.get<string>("app_version"), pt2.get<string>("first_crash_date_time"),
				pt2.get<string>("last_crash_date_time"), pt2.get<string>("crash_context_digest"), pt2.get<string>("crash_context"));
			
			//InsertDeveloperInfo(con, p1.get<string>("crash_context_digest"));
		}

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


private:
	//执行插入语句，向数据中插入异常数据信息
	void InsertDataToDataBase(sql::Connection *con, const string crash_id
		, const string fixed, const string app_version, const string first_crash_date_time
		, const string last_crash_date_time, const string crash_context_digest, const string crash_context)
	{
		//从异常信息中提取出开发者的id
		//第一次出现等号的位置
		int equalIndex = crash_context_digest.find_first_of("=");
		//第一次出现左括号的位置
		int leftIndex = crash_context_digest.find_first_of("(");
		//开发者id的长度
		int idLength = leftIndex - 1 - equalIndex;
		string id;
		if (idLength > 0)
			id = crash_context_digest.substr(equalIndex + 1, idLength);
		else
			id = "";

		sql::PreparedStatement *pstmt;
		pstmt = con->prepareStatement("INSERT INTO ErrorInfo(crash_id,developerid,fixed,app_version,first_crash_date_time,last_crash_date_time,crash_context_digest,crash_context) VALUES(?,?,?,?,?,?,?,?)");
		
		pstmt->setString(1, crash_id);
		pstmt->setString(2, id);
		pstmt->setString(3, fixed);
		pstmt->setString(4, app_version);
		pstmt->setString(5, first_crash_date_time);
		pstmt->setString(6, last_crash_date_time);
		pstmt->setString(7, crash_context_digest);
		pstmt->setString(8, crash_context);

		pstmt->execute();

		delete pstmt;
	}

	int GetErrorList()
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
			//writeFile(errorList.c_str(), "receiveData.txt");
		}
		catch (std::exception& e)
		{
			errorList = "";
			std::cout << e.what() << std::endl;
			return -4;
		}
		return 0;
	}

	int GetLoginToken()
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

	void writeFile(const char *src, const char *fileName)
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
	void InsertDeveloperInfo(sql::Connection *con, string info)
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

	string host;
	string loginPage;
	string analyzePage;
	string data;
	string origin;
	string appkey;
	string start_date;
	string end_date;

	string token;
	string errorList;
};