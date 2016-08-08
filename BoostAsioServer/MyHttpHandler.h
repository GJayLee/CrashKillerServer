#pragma once

#include "MyDatabaseHandler.h"

using std::string;

/*������ƽ̨��ȡ���ݣ�HTTP��������,�Լ����ݿ�Ĳ���
�ѻ�ȡ��������д�����ݿ���*/
class MyHttpHandler :public MyDatabaseHandler
{
public:
	//���캯��
	MyHttpHandler();
	MyHttpHandler(string a, string s, string e);
	~MyHttpHandler();

	//�������������ݣ���д�����ݿ�
	void excuteAction();

private:
	//��ȡ�쳣�����б�
	int GetErrorList(string offset, string appkey, string start_date, string end_date);
	//��ȡ��½���ѵ�token
	int GetLoginToken();
	////�����쳣�����б���д�����ݿ���
	//void ParseJsonAndInsertToDatabase(string tableName);
	////ִ���쳣����
	//void excuteCrashClassfy(string tableName);
	////ִ�в�����䣬�������в����쳣������Ϣ
	//void InsertDataToDataBase(string tableName, const string crash_id
	//	, const string fixed, const string app_version, const string first_crash_date_time
	//	, const string last_crash_date_time, const string crash_context_digest, const string crash_context);
	////�����쳣��Ϣ��ȡ����ģ����Ϣ
	//void InsertModulesInfo(string crash_context);
	////�����쳣��Ϣ��ȡ��������Ϣ
	//void InsertDeveloperInfo(string info);
	////�����쳣�ж�ջ��ģ����Ϣ���ܷ����쳣�����ط���Ŀ���������
	//string AutoDistributeCrash(string crash_context);
	////��ͬһ���˵��쳣��Ϣ���г����ķ���
	//void AutoClassifyCrash(string tableName, string developer);
	
	string host;                  //����
	string loginPage;             //��½��ַ
	string analyzePage;           //��ȡ�쳣���ݽӿ���ַ
	string data;                  //��½��Ϣ
	string origin;            

	string token;                 //��½�ɹ�ʱ���ص�token
	string errorList;             //�쳣�����б�JSON��ʽ
};