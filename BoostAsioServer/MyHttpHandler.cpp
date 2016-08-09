/*������ƽ̨��ȡ���ݣ�HTTP��������,
ͬʱ�ѻ�ȡ��������д�����ݿ���*/

#include<unordered_map>
#include<unordered_set>
#include <fstream>
#include <boost/asio.hpp>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>

#include "generic.h"
#include "MyHttpHandler.h"

#define BOOST_SPIRIT_THREADSAFE

using namespace boost::property_tree;
using namespace boost::gregorian;
using boost::asio::ip::tcp;

//ÿ����������ѻ�ȡ���ݵ��������,�ַ�������ƴ��HTTP�������������ж��Ƿ�ﵽ���ֵ
const string LIMIT_NUM = "50";
const int MAX_LIMIT = 50;

//����Ĭ�Ϲ��캯����ʼ����̬��������Ŀ������Ϣ
std::vector<string> MyDatabaseHandler::tables;
std::vector<string> MyDatabaseHandler::appkeys;
std::vector<string> MyDatabaseHandler::starts;
std::vector<string> MyDatabaseHandler::ends;
string MyDatabaseHandler::projectConfigureInfoInJson;
std::unordered_map<string, string> MyDatabaseHandler::appkey_tables;

MyHttpHandler::MyHttpHandler()
{
	//����Header��Ϣ
	host = "myou.cvte.com";
	loginPage = "http://myou.cvte.com/api/in/auth/login";
	data = "{\"email\":\"lindexi@cvte.com\",\"password\":\"11111111\"}";
	origin = "http://myou.cvte.com";

	InitDatabaseTabelByAppkey();
}

MyHttpHandler::~MyHttpHandler()
{
}

/*
public
*/
//��ʼִ�в����������������쳣���ݣ���д�����ݿ�
void MyHttpHandler::excuteAction()
{
	for (int i = 0; i < appkeys.size(); i++)
	{
		//ͳ�Ƹ������Ƿ��Ѵﵽ���ֵ��limit
		int count = 0;
		//��һ���������������ݣ��������Ϊ50��
		errorList = "";
		token = "";
		GetLoginToken();
		std::cout << "Get Token finish!" << std::endl;
		//��һ������ƫ����Ϊ0���ӵ�һ��λ�ÿ�ʼ��ȡ
		GetErrorList("0", appkeys[i], starts[i], ends[i]);
		//������һ�λ�ȡ�����ݣ���д�����ݿ���
		ParseJsonAndInsertToDatabase(count, tables[i], errorList);

		//����ƫ����
		int offset = 1;
		//������մﵽ������ƣ����޸�offset����������鿴�Ƿ��б������
		while (count >= MAX_LIMIT)
		{
			errorList = "";
			std::stringstream offsetStr;
			int offsetIndex = MAX_LIMIT*offset;
			offsetStr << offsetIndex;
			//����ƫ��������ƫ��������ʼ��ȡ
			GetErrorList(offsetStr.str(), appkeys[i], starts[i], ends[i]);
			ParseJsonAndInsertToDatabase(count, tables[i], errorList);
			++offset;
		}
		std::cout << "Get Error List finish!" << std::endl;
		excuteCrashClassfy(tables[i]);
	}
}

/*
private
*/
//��myou��ȡ�쳣�б�
int MyHttpHandler::GetErrorList(string offset, string appkey, string start_date, string end_date)
{
	analyzePage = "http://myou.cvte.com/api/in/analyze/" + appkey
		+ "/crash_list?platform=windows_app&app_version=&start_date=" + start_date
		+ "&end_date=" + end_date
		+ "&limit=" + LIMIT_NUM + "&offset=" + offset + "&fixed=false";
	try
	{
		boost::asio::io_service io_service;
		//���io_service���ڸ��õ����
		if (io_service.stopped())
			io_service.reset();

		// ��dnsȡ�������µ�����ip
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(host, "http");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

		// �������ӵ����е�ĳ��ipֱ���ɹ� 
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
		// ������������ط�200����Ϊ�д�,��֧��301/302����ת
		if (status_code != 200)
		{
			errorList = "";
			std::cout << "Response returned with status code != 200 " << std::endl;
			return status_code;
		}

		// ��˵�еİ�ͷ���Զ�������
		std::string header;
		std::vector<string> headers;
		while (std::getline(response_stream, header) && header != "\r")
			headers.push_back(header);

		// ��ȡ����ʣ�µ�������Ϊ����
		boost::system::error_code error;
		while (boost::asio::read(socket, response,
			boost::asio::transfer_at_least(1), error))
		{
			std::istream is(&response);
			is.unsetf(std::ios_base::skipws);
			errorList.append(std::istream_iterator<char>(is), std::istream_iterator<char>());
			//std::cout << &response;
		}
		//������д�뵽�ļ���
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
//��ȡ��½��token
int MyHttpHandler::GetLoginToken()
{
	try
	{
		boost::asio::io_service io_service;
		//���io_service���ڸ��õ����
		if (io_service.stopped())
			io_service.reset();

		// ��dnsȡ�������µ�����ip
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(host, "http");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

		// �������ӵ����е�ĳ��ipֱ���ɹ� 
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
		// ������������ط�200����Ϊ�д�,��֧��301/302����ת
		if (status_code != 200)
		{
			token = "";
			std::cout << "Response returned with status code != 200 " << std::endl;
			return status_code;
		}

		// ��˵�еİ�ͷ���Զ�������
		std::string header;
		std::vector<string> headers;
		while (std::getline(response_stream, header) && header != "\r")
			headers.push_back(header);

		// ��ȡ����ʣ�µ�������Ϊ����
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
	}
	catch (std::exception& e)
	{
		token = "";
		std::cout << e.what() << std::endl;
		return -4;
	}
	return 0;
}