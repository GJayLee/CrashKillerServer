#pragma once

//�������ݿ�
#include<cppconn\driver.h>
#include<cppconn\exception.h>
#include <cppconn/resultset.h> 
#include <cppconn/statement.h>
#include<mysql_connection.h>
#include <cppconn/prepared_statement.h>

using std::string;

/*������ƽ̨��ȡ���ݣ�HTTP��������,�Լ����ݿ�Ĳ���
�ѻ�ȡ��������д�����ݿ���*/
class MyHttpHandler
{
public:
	//���캯��
	MyHttpHandler();
	MyHttpHandler(string a, string s, string e);
	~MyHttpHandler();

	//������ĿID
	void setAppKey(string key);
	//�����쳣���ݵĿ�ʼ����
	void setStartDate(string date);
	//�����쳣���ݵĽ�������
	void setEndDate(string date);
	//�������������ݣ���д�����ݿ�
	void excuteAction();
	//��ͬһ���˵��쳣��Ϣ���г����ķ���
	void AutoClassifyCrash(string developer);

private:
	//��ȡ�쳣�����б�
	int GetErrorList(string offset, string appkey, string start_date, string end_date);
	//��ȡ��½���ѵ�token
	int GetLoginToken();
	//�����쳣�����б���д�����ݿ���
	void ParseJsonAndInsertToDatabase(string tableName);

	//�����ⲿ�����ļ��е�appkey�����ݿ��д�����Ӧ�ı�
	void InitDatabaseTabelByAppkey();
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
	//��ͬһ���˵��쳣��Ϣ���г����ķ���
	//void AutoClassifyCrash(string developer);
	
	//���ַ����Աȣ��������ƶ�
	int Levenshtein(string str1, string str2);
	//������������Сֵ
	int min(int a, int b, int c);

	//������д�뵽�ı��ļ��з���鿴������
	void writeFile(const char *src, const char *fileName);

	string host;                  //����
	string loginPage;             //��½��ַ
	string analyzePage;           //��ȡ�쳣���ݽӿ���ַ
	string data;                  //��½��Ϣ
	string origin;            
	//string appkey;                //��ĿID
	//string start_date;            //�쳣���ݵĿ�ʼ����
	//string end_date;              //�쳣���ݵĽ�������

	std::vector<string> tables;
	std::vector<string> appkeys;
	std::vector<string> starts;
	std::vector<string> ends;

	string token;                 //��½�ɹ�ʱ���ص�token
	string errorList;             //�쳣�����б�JSON��ʽ
								  
	int count;                    //ͳ�Ƹ������Ƿ��Ѵﵽ���ֵ��limit

	//�������ݿ�
	sql::Driver *dirver;
	sql::Connection *con;
};