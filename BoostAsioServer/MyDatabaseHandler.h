#pragma once
//�������ݿ�
#include<cppconn\driver.h>
#include<cppconn\exception.h>
#include <cppconn/resultset.h> 
#include <cppconn/statement.h>
#include<mysql_connection.h>
#include <cppconn/prepared_statement.h>

//boost
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>
#include <boost\regex.hpp>

#include<unordered_map>
#include<unordered_set>
 
using std::string;

/*
���ݿ�����࣬��Ҫ�Ƕ����ݿ������ɾ�Ĳ�
*/

class MyDatabaseHandler
{
public:
	//���캯��
	MyDatabaseHandler();
	//��������
	~MyDatabaseHandler();
	
	//��ȡ�ⲿ��Ŀ�����ļ�
	/*void GetAppkeys(std::vector<string> &res);
	void GetTables(std::vector<string> &res);
	void GetStarts(std::vector<string> &res);
	void GetEnds(std::vector<string> &res);
	void GetAppkeysTables(std::unordered_map<string, string> &res);*/

	/*
	����˺Ϳͻ��˶����ݿ�Ĳ���
	*/
	//��ȡ�ⲿ�����ļ���Ϣ���������ݿ��д�����Ӧ�ı�
	void InitDatabaseTabelByAppkey();
	//��ͬһ���˵��쳣��Ϣ���г����ķ���
	void AutoClassifyCrash(string tableName, string developer);
	//���������쳣��������������ƶ�
	bool CalculateCos(std::vector<int> a, std::vector<int> b);

	/*
	����˵����ݿ����
	*/
	//���������ѻ�ȡ�����ݣ����������ݿ��У�����������и��쳣����Ϣ�����ٲ���
	//ֻ�����»�ȡ�����쳣��Ϣ
	void MyDatabaseHandler::ParseJsonAndInsertToDatabase(int &count, string tableName, string errorList);
	//ִ���쳣����
	void MyDatabaseHandler::excuteCrashClassfy(string tableName);
	//ִ�в�����䣬�������в����쳣������Ϣ
	void InsertDataToDataBase(string tableName, const string crash_id
		, const string fixed, const string app_version, const string first_crash_date_time
		, const string last_crash_date_time, const string crash_context_digest, const string crash_context);
	//�����쳣��Ϣ��ȡ����ģ����Ϣ
	void InsertModulesInfo(string crash_context);
	//�����쳣��Ϣ��ȡ��������Ϣ
	void InsertDeveloperInfo(string info);
	//�����쳣�ж�ջ��ģ����Ϣ���ܷ����쳣�����ط���Ŀ���������
	string AutoDistributeCrash(string crash_context);

	/*
	�ͻ��˵����ݿ����
	*/
	//�����ݿ��л�ȡ��������Ϣ��ת��ΪJSON��ʽ
	void GetDeveloperInfo(string &developerInfo);
	//�����ݿ��л�ȡ���ݲ�������תΪJSON��ʽ��
	void TransferDataToJson(std::vector<string> &crashInfo, string appkey);
	//�ӿͻ����յ�������Ϣ���������ݿ�
	void UpdateDatabase(string clientData);

protected:
	//�ⲿ��Ŀ������Ϣ
	static std::vector<string> tables;
	static std::vector<string> appkeys;
	static std::vector<string> starts;
	static std::vector<string> ends;
	static string projectConfigureInfoInJson;
	static std::unordered_map<string, string> appkey_tables;

private:
	//�������ݿ�
	sql::Driver *dirver;
	sql::Connection *con;
};