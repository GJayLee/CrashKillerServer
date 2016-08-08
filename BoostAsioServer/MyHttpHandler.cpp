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

MyHttpHandler::MyHttpHandler()
{
	//����Header��Ϣ
	host = "myou.cvte.com";
	loginPage = "http://myou.cvte.com/api/in/auth/login";
	data = "{\"email\":\"lindexi@cvte.com\",\"password\":\"11111111\"}";
	origin = "http://myou.cvte.com";

	InitDatabaseTabelByAppkey();
}
MyHttpHandler::MyHttpHandler(string a, string s, string e)
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

////���������ѻ�ȡ�����ݣ����������ݿ��У�����������и��쳣����Ϣ�����ٲ���
////ֻ�����»�ȡ�����쳣��Ϣ
//void MyHttpHandler::ParseJsonAndInsertToDatabase(string tableName)
//{
//	//ͳ�Ƹ������Ƿ��Ѵﵽ���ֵ��limit
//	count = 0;
//	//�����ȡ���쳣����Ϊ�գ���֤���ѽ������
//	if (errorList == "")
//		return;
//
//	con->setAutoCommit(0);
//	ptree pt, pt1, pt2;
//	std::stringstream stream;
//	stream << errorList;
//	read_json<ptree>(stream, pt);
//
//	pt1 = pt.get_child("data");
//
//	for (ptree::iterator it = pt1.begin(); it != pt1.end(); ++it)
//	{
//		pt2 = it->second;
//		//std::cout << "app_version:" << p1.get<string>("app_version") << std::endl;
//		InsertDataToDataBase(tableName, pt2.get<string>("crash_id"), pt2.get<string>("fixed"),
//			pt2.get<string>("app_version"), pt2.get<string>("first_crash_date_time"),
//			pt2.get<string>("last_crash_date_time"), pt2.get<string>("crash_context_digest"), pt2.get<string>("crash_context"));
//
//		count++;
//		//����
//		//InsertModulesInfo(pt2.get<string>("crash_context"));
//		//InsertDeveloperInfo(p1.get<string>("crash_context_digest"));
//	}
//
//	con->commit();
//}
//
////ִ���쳣����
//void MyHttpHandler::excuteCrashClassfy(string tableName)
//{
//	sql::PreparedStatement *pstmt;
//	string sqlStatement = "select developer from " + tableName + " group by developer";
//	pstmt = con->prepareStatement(sqlStatement);
//	sql::ResultSet *res;
//	res = pstmt->executeQuery();
//	while (res->next())
//	{
//		AutoClassifyCrash(tableName, res->getString("developer"));
//	}
//	delete pstmt;
//	delete res;
//}
////�����ⲿ�����ļ��е�appkey�����ݿ��д�����Ӧ�ı�
//void MyHttpHandler::InitDatabaseTabelByAppkey()
//{
//	//��ȡ�����ļ���appkey��Ϣ
//	std::ifstream t("ProjectsConfigure.txt");
//	if (!t)
//	{
//		std::cout << "����ĿAppkey�����ļ�ʧ�ܣ�" << std::endl;
//		return;
//	}
//	std::stringstream buffer;
//	buffer << t.rdbuf();
//
//	try
//	{
//		ptree pt, pt1, pt2;
//		read_json<ptree>(buffer, pt);
//		pt1 = pt.get_child("projects");
//		for (ptree::iterator it = pt1.begin(); it != pt1.end(); ++it)
//		{
//			pt2 = it->second;
//			appkeys.push_back(pt2.get<string>("appkey"));
//			tables.push_back(pt2.get<string>("tableName"));
//			starts.push_back(pt2.get<string>("start_date"));
//			ends.push_back(pt2.get<string>("end_date"));
//		}
//
//		con->setAutoCommit(0);
//		//����appkey��������ݿ��в����ڸ�appkey�ı��򴴽�
//		for (int i = 0; i < appkeys.size(); i++)
//		{
//			sql::PreparedStatement *pstmt;
//			string sqlStatement;
//			string str1 = "CREATE TABLE if not EXISTS " + tables[i] + " (crash_id varchar(255) PRIMARY KEY NOT NULL,";
//			string str2 = "developer varchar(255) DEFAULT NULL, fixed VARCHAR(255) DEFAULT NULL,crash_type VARCHAR(255) DEFAULT NULL,app_version VARCHAR(255) DEFAULT NULL,";
//			string str3 = "first_crash_date_time VARCHAR(255) DEFAULT NULL,last_crash_date_time VARCHAR(255) DEFAULT NULL,";
//			string str4 = "crash_context_digest VARCHAR(255) DEFAULT NULL,crash_context text DEFAULT NULL) ENGINE = InnoDB DEFAULT CHARSET = utf8";
//			sqlStatement = str1 + str2 + str3 + str4;
//			pstmt = con->prepareStatement(sqlStatement);
//			pstmt->execute();
//
//			delete pstmt;
//		}
//		con->commit();
//	}
//	catch (std::exception& e)
//	{
//		std::cout << "���������ļ�����" << std::endl;
//	}
//
//}
////�����쳣�ж�ջ��ģ����Ϣ��Ȩ�����ܷ����쳣�����ط���Ŀ���������
//string MyHttpHandler::AutoDistributeCrash(string crash_context)
//{
//	string developerName = "";
//	string moduleName = "";
//	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
//	boost::regex reg(regStr);
//	boost::smatch m;
//	std::unordered_map<string, int> developerWeight;
//	int maxWeight = 0;
//	string maxId;
//
//	//�������ݿ���Ϣ
//	sql::PreparedStatement *pstmt;
//	sql::ResultSet *res;
//	string sqlStatement;
//
//	while (boost::regex_search(crash_context, m, reg))
//	{
//		boost::smatch::iterator it = m.begin();
//		moduleName = it->str();
//
//
//		sqlStatement = "select developer_id from moduleinfo where module_name = \"" + moduleName + "\"";
//		//��errorinfo���л�ȡ������Ϣ
//		pstmt = con->prepareStatement(sqlStatement);
//		res = pstmt->executeQuery();
//
//		string developer_id;
//		while (res->next())
//			developer_id = res->getString("developer_id");
//
//		delete pstmt;
//		delete res;
//
//		if (developerWeight.find(developer_id) == developerWeight.end())
//			developerWeight[developer_id] = 1;
//		else
//		{
//			//�ҳ�Ȩ�����Ŀ����ߣ���Ϊһ���쳣�п����漰����ģ�飬ÿ��ģ�鶼�ɲ�ͬ
//			//������ʵ�֣�һ��ģ��������ߵ�Ȩ�ؼ�1
//			developerWeight[developer_id]++;
//			if (developerWeight[developer_id] > maxWeight)
//			{
//				maxId = developer_id;
//				maxWeight = developerWeight[developer_id];
//			}
//		}
//		crash_context = m.suffix().str();
//	}
//
//	sqlStatement = "select Name from developer where ID = \"" + maxId + "\"";
//	pstmt = con->prepareStatement(sqlStatement);
//	res = pstmt->executeQuery();
//
//	while (res->next())
//		developerName = res->getString("Name");
//
//	delete pstmt;
//	delete res;
//
//	return developerName;
//}
//
////��ͬһ���˵��쳣��Ϣ���г����ķ���
//void MyHttpHandler::AutoClassifyCrash(string tableName, string developer)
//{
//	sql::PreparedStatement *pstmt;
//	string sqlStatement = "select crash_id, crash_context from " + tableName + " where developer = \"" + developer + "\"";
//	pstmt = con->prepareStatement(sqlStatement);
//	sql::ResultSet *res;
//	res = pstmt->executeQuery();
//
//	//�쳣id�ļ���
//	std::vector<string> crash_id;
//	std::unordered_map<string, bool> hasClassy;
//	//ÿ���쳣������Щģ��
//	std::unordered_map<string, std::unordered_set<string>> crash_modules;
//	//�ÿ����߸�����Щģ��
//	std::unordered_set<string> crash_totalModules;
//
//	while (res->next())
//	{
//		crash_id.push_back(res->getString("crash_id"));
//		hasClassy[res->getString("crash_id")] = false;
//		const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2,9}";
//		boost::regex reg(regStr);
//		boost::smatch m;
//		string crash_context = res->getString("crash_context");
//		while (boost::regex_search(crash_context, m, reg))
//		{
//			boost::smatch::iterator it = m.begin();
//			crash_totalModules.insert(it->str());
//			crash_modules[res->getString("crash_id")].insert(it->str());
//			crash_context = m.suffix().str();
//		}
//	}
//
//	delete pstmt;
//	delete res;
//	
//	//��ʼ��ÿ���쳣��ģ������
//	std::unordered_map<string, std::vector<int>> crash_vectors;
//	std::unordered_set<string>::iterator iterTotalModules = crash_totalModules.begin();
//	for (; iterTotalModules != crash_totalModules.end(); ++iterTotalModules)
//	{
//		std::unordered_map<string, std::unordered_set<string>>::iterator iter = crash_modules.begin();
//		for (; iter != crash_modules.end(); ++iter)
//		{
//			if (iter->second.find((*iterTotalModules)) == iter->second.end())
//				crash_vectors[iter->first].push_back(0);
//			else
//				crash_vectors[iter->first].push_back(1);
//		}
//	}
//	//�쳣�����ͣ������쳣�������������ƶ��ж������Ƿ���ͬһ���쳣
//	int type = 0;
//	std::unordered_map<int, std::vector<string>> crash_types;
//	std::unordered_map<string, std::vector<int>>::iterator iter = crash_vectors.begin();
//	for (int i = 0; i < crash_id.size(); i++)
//	{
//		if (!hasClassy[crash_id[i]])
//		{
//			crash_types[type].push_back(crash_id[i]);
//			if (crash_vectors.find(crash_id[i]) == crash_vectors.end())
//				continue;
//		}
//		else
//			continue;
//		for (int j = i + 1; j < crash_id.size(); j++)
//		{
//			if (crash_vectors.find(crash_id[j]) == crash_vectors.end())
//				continue;
//			if (CalculateCos(crash_vectors[crash_id[i]], crash_vectors[crash_id[j]]))
//			{
//				crash_types[type].push_back(crash_id[j]);
//				hasClassy[crash_id[j]] = true;
//			}
//		}
//		++type;
//	}
//
//	//�����쳣�����ͣ��������������ݿ���
//	con->setAutoCommit(0);
//	for (int i = 0; i < type; i++)
//	{
//		for (int j = 0; j < crash_types[i].size(); j++)
//		{
//			std::stringstream ss;
//			ss << i;
//			sqlStatement = "update " + tableName + " set crash_type=\"" + ss.str() + "\" where crash_id = \"" + crash_types[i][j] + "\"";
//			pstmt = con->prepareStatement(sqlStatement);
//			pstmt->execute();
//			delete pstmt;
//		}
//	}
//	con->commit();
//}
//
////���쳣��ջ��Ϣ����ȡ��ģ����Ϣ,���ԣ���ʼ��
//void MyHttpHandler::InsertModulesInfo(string crash_context)
//{
//	//�ַ���ƥ��Cvte����Ϣ
//	//������ʽƥ��̶���ʽ���ַ���
//	//const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){1,4}";
//	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
//	boost::regex reg(regStr);
//	boost::smatch m;
//	while (boost::regex_search(crash_context, m, reg))
//	{
//		//����ģ����Ϣ
//		boost::smatch::iterator it = m.begin();
//		//std::cout << it->str() << std::endl;
//		sql::PreparedStatement *pstmt;
//		//����ģ����Ϣ��������ݿ�������ͬ���������ݣ���������ݿ���Ϣ
//		pstmt = con->prepareStatement("INSERT IGNORE INTO moduleInfo(module_name) VALUES(?)");
//		pstmt->setString(1, it->str());
//		pstmt->execute();
//		delete pstmt;
//
//		crash_context = m.suffix().str();
//	}
//}
//
////�������ݵ����ݿ���
//void MyHttpHandler::InsertDataToDataBase(string tableName, const string crash_id, const string fixed, const string app_version
//	, const string first_crash_date_time, const string last_crash_date_time, const string crash_context_digest,
//	const string crash_context)
//{
//	//�����ж����ݿ����Ƿ��Ѵ��ڸ��쳣����
//	sql::PreparedStatement *pstmt;
//	string sqlStatement = "select crash_context from " + tableName + " where crash_id = \"" + crash_id + "\"";
//	pstmt = con->prepareStatement(sqlStatement);
//	sql::ResultSet *res;
//	res = pstmt->executeQuery();
//	bool isExisted = false;
//	while (res->next())
//		isExisted = true;
//
//	delete pstmt;
//	delete res;
//	//����������ӵ����ݿ���
//	if (!isExisted)
//	{
//		//�����쳣��Ϣ�е�ģ����Ϣ���쳣���ܷ����������
//		string developerName = AutoDistributeCrash(crash_context);
//
//		//ʹ��replace��䣬������ݿ�������ͬ���������ݣ���������ݿ���Ϣ
//		//pstmt = con->prepareStatement("REPLACE INTO ErrorInfo(crash_id,developerid,fixed,app_version,first_crash_date_time,last_crash_date_time,crash_context_digest,crash_context) VALUES(?,?,?,?,?,?,?,?)");
//		sqlStatement = "INSERT INTO " + tableName 
//			+ "(crash_id,developer,fixed,app_version,first_crash_date_time,last_crash_date_time,crash_context_digest,crash_context) VALUES(?,?,?,?,?,?,?,?)";
//		pstmt = con->prepareStatement(sqlStatement);
//
//		pstmt->setString(1, crash_id);
//		pstmt->setString(2, developerName);
//		pstmt->setString(3, fixed);
//		pstmt->setString(4, app_version);
//		pstmt->setString(5, first_crash_date_time);
//		pstmt->setString(6, last_crash_date_time);
//		pstmt->setString(7, crash_context_digest);
//		pstmt->setString(8, crash_context);
//
//		pstmt->execute();
//
//		delete pstmt;
//	}
//}
//
////�����쳣��Ϣ��ȡ��������Ϣ������
//void MyHttpHandler::InsertDeveloperInfo(string info)
//{
//	//��һ�γ��ֵȺŵ�λ��
//	int equalIndex = info.find_first_of("=");
//	//��һ�γ��������ŵ�λ��
//	int leftIndex = info.find_first_of("(");
//	//��һ�γ��������ŵ�λ��
//	int rightIndex = info.find_first_of(")");
//	//������id�ĳ���
//	int idLength = leftIndex - 1 - equalIndex;
//	string id;
//	if (idLength > 0)
//		id = info.substr(equalIndex + 1, idLength);
//	else
//		return;
//	//���������ֵĳ���
//	string name;
//	int nameLeghth = rightIndex - 1 - leftIndex;
//	if (nameLeghth > 0)
//		name = info.substr(leftIndex + 1, nameLeghth);
//	else
//		return;
//
//	sql::PreparedStatement *pstmt;
//	pstmt = con->prepareStatement("INSERT IGNORE INTO developer(ID,Name) VALUES(?,?)");
//
//	pstmt->setString(1, id);
//	pstmt->setString(2, name);
//
//	pstmt->execute();
//
//	delete pstmt;
//}