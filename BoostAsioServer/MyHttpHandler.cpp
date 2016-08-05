/*������ƽ̨��ȡ���ݣ�HTTP��������,
ͬʱ�ѻ�ȡ��������д�����ݿ���*/

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

//ÿ����������ѻ�ȡ���ݵ��������,�ַ�������ƴ��HTTP�������������ж��Ƿ�ﵽ���ֵ
const string LIMIT_NUM = "50";
const int MAX_LIMIT = 50;

//������ƽ̨��ȡ�쳣����ʱ��Ĭ��appKey��start_date��end_date
const string defaultAppKey = "9e4b010a0d51f6e020ead6ce37bad33896a00f90";
const string defaultStartDate = "2016-01-1";
const string defaultEndDate = "2016-07-26";

//UTF-8תGB2312������
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

//GB2312תUTF-8������
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
	
	//�������ݿ�
	dirver = get_driver_instance();
	con = dirver->connect("localhost", "root", "123456");
	if (con == NULL)
	{
		std::cout << "�����������ݿ⣡" << std::endl;
		return;
	}
	//ѡ��CrashKiller���ݿ�
	con->setSchema("CrashKiller");
}
MyHttpHandler::MyHttpHandler(string a, string s, string e) : appkey(a), start_date(s), end_date(e)
{
	//����Header��Ϣ
	host = "myou.cvte.com";
	loginPage = "http://myou.cvte.com/api/in/auth/login";
	data = "{\"email\":\"lindexi@cvte.com\",\"password\":\"11111111\"}";
	origin = "http://myou.cvte.com";

	//�������ݿ�
	dirver = get_driver_instance();
	con = dirver->connect("localhost", "root", "123456");
	if (con == NULL)
	{
		std::cout << "�����������ݿ⣡" << std::endl;
		return;
	}
	//ѡ��CrashKiller���ݿ�
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

//���������ѻ�ȡ�����ݣ����������ݿ��У�����������и��쳣����Ϣ�����ٲ���
//ֻ�����»�ȡ�����쳣��Ϣ
void MyHttpHandler::ParseJsonAndInsertToDatabase()
{
	//sql::Statement *stmt;
	//stmt = con->createStatement();
	//stmt->execute("delete from errorinfo");
	//delete stmt;
	
	//ͳ�Ƹ������Ƿ��Ѵﵽ���ֵ��limit
	count = 0;
	//�����ȡ���쳣����Ϊ�գ���֤���ѽ������
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
		//����
		//InsertModulesInfo(pt2.get<string>("crash_context"));
		//InsertDeveloperInfo(p1.get<string>("crash_context_digest"));
	}

	con->commit();
}

//�������������ݣ���д�����ݿ�
void MyHttpHandler::excuteAction()
{
	setAppKey(defaultAppKey);
	setStartDate(defaultStartDate);
	setEndDate(defaultEndDate);
	//PostHttpRequest();
	
	//��һ���������������ݣ��������Ϊ50��
	errorList = "";
	GetLoginToken();
	std::cout << "Get Token finish!" << std::endl;
	//��һ������ƫ����Ϊ0���ӵ�һ��λ�ÿ�ʼ��ȡ
	GetErrorList("0");
	//������һ�λ�ȡ�����ݣ���д�����ݿ���
	ParseJsonAndInsertToDatabase();

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
		GetErrorList(offsetStr.str());
		ParseJsonAndInsertToDatabase();
		++offset;
	}
	std::cout << "Get Error List finish!" << std::endl;
}

//�����쳣�ж�ջ��ģ����Ϣ��Ȩ�����ܷ����쳣�����ط���Ŀ���������
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

	//�������ݿ���Ϣ
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;

	while (boost::regex_search(crash_context, m, reg))
	{
		boost::smatch::iterator it = m.begin();
		moduleName = it->str();


		sqlStatement = "select developer_id from moduleinfo where module_name = \"" + moduleName + "\"";
		//��errorinfo���л�ȡ������Ϣ
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
			//�ҳ�Ȩ�����Ŀ����ߣ���Ϊһ���쳣�п����漰����ģ�飬ÿ��ģ�鶼�ɲ�ͬ
			//������ʵ�֣�һ��ģ��������ߵ�Ȩ�ؼ�1
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
		//��errorinfo���л�ȡ������Ϣ
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

//��ͬһ���˵��쳣��Ϣ���г����ķ���,δ���
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

//���ַ����Աȣ��������ƶ�
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
	//�ٷֱ�
	similarity *= 100;

	for (int i = 0; i < len1 + 1; i++)
		delete[] dif[i];
	
	return (int)similarity;
}

//���쳣��ջ��Ϣ����ȡ��ģ����Ϣ,���ԣ���ʼ��
void MyHttpHandler::InsertModulesInfo(string crash_context)
{
	//�ַ���ƥ��Cvte����Ϣ
	/*int pos=crash_context.find("Cvte");
	while (pos != string::npos)
	{
		int i = pos;
		while (crash_context[i] != '(')
			i++;

		sql::PreparedStatement *pstmt;
		//����ģ����Ϣ��������ݿ�������ͬ���������ݣ���������ݿ���Ϣ
		pstmt = con->prepareStatement("INSERT IGNORE INTO moduleInfo(module_name) VALUES(?)");
		pstmt->setString(1, crash_context.substr(pos,i-pos));
		pstmt->execute();
		delete pstmt;

		pos = crash_context.find("Cvte", i + 1);
	}*/

	//������ʽƥ��̶���ʽ���ַ���
	//const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){1,4}";
	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
	boost::regex reg(regStr);
	boost::smatch m;
	while (boost::regex_search(crash_context, m, reg))
	{
		//����ģ����Ϣ
		/*for (boost::smatch::iterator it = m.begin(); it != m.end(); it+=2)
		{
			std::cout << it->str() << std::endl;
			sql::PreparedStatement *pstmt;
			//����ģ����Ϣ��������ݿ�������ͬ���������ݣ���������ݿ���Ϣ
			pstmt = con->prepareStatement("INSERT IGNORE INTO moduleInfo(module_name) VALUES(?)");
			pstmt->setString(1, it->str());
			pstmt->execute();
			delete pstmt;
		}*/

		boost::smatch::iterator it = m.begin();
		//std::cout << it->str() << std::endl;
		sql::PreparedStatement *pstmt;
		//����ģ����Ϣ��������ݿ�������ͬ���������ݣ���������ݿ���Ϣ
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
	////���쳣��Ϣ����ȡ�������ߵ�id
	////��һ�γ��ֵȺŵ�λ��
	//int equalIndex = crash_context_digest.find_first_of("=");
	////��һ�γ��������ŵ�λ��
	//int leftIndex = crash_context_digest.find_first_of("(");
	////������id�ĳ���
	//int idLength = leftIndex - 1 - equalIndex;
	//string id;
	//if (idLength > 0)
	//	id = crash_context_digest.substr(equalIndex + 1, idLength);
	//else
	//	id = "";

	//�����ж����ݿ����Ƿ��Ѵ��ڸ��쳣����
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
	//����������ӵ����ݿ���
	if (!isExisted)
	{
		//�����쳣��Ϣ�е�ģ����Ϣ���쳣���ܷ����������
		string developerName = AutoDistributeCrash(crash_context);

		//ʹ��replace��䣬������ݿ�������ͬ���������ݣ���������ݿ���Ϣ
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
		//��utf8��ʽת��ΪGB2312
		//char *myData;
		//myData = U2G(errorList.c_str());
		/*for (int i = 0; i < strlen(myData); i++)
		{
		std::cout << myData[i];
		}
		std::cout << std::endl;*/
		//������д�뵽�ļ���
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

//������������Сֵ
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

//�����쳣��Ϣ��ȡ��������Ϣ
void MyHttpHandler::InsertDeveloperInfo(string info)
{
	//��һ�γ��ֵȺŵ�λ��
	int equalIndex = info.find_first_of("=");
	//��һ�γ��������ŵ�λ��
	int leftIndex = info.find_first_of("(");
	//��һ�γ��������ŵ�λ��
	int rightIndex = info.find_first_of(")");
	//������id�ĳ���
	int idLength = leftIndex - 1 - equalIndex;
	string id;
	if (idLength > 0)
		id = info.substr(equalIndex + 1, idLength);
	else
		return;
	//���������ֵĳ���
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