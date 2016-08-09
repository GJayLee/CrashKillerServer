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
	~MyHttpHandler();

	//�������������ݣ���д�����ݿ�
	void excuteAction();

private:
	//��ȡ�쳣�����б�
	int GetErrorList(string offset, string appkey, string start_date, string end_date);
	//��ȡ��½���ѵ�token
	int GetLoginToken();
	
	string host;                  //����
	string loginPage;             //��½��ַ
	string analyzePage;           //��ȡ�쳣���ݽӿ���ַ
	string data;                  //��½��Ϣ
	string origin;            

	string token;                 //��½�ɹ�ʱ���ص�token
	string errorList;             //�쳣�����б�JSON��ʽ
};