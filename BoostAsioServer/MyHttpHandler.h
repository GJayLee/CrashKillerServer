#pragma once

#include "MyDatabaseHandler.h"

using std::string;

/*从萌友平台获取数据，HTTP请求处理类,以及数据库的操作
把获取到的数据写入数据库中*/
class MyHttpHandler :public MyDatabaseHandler
{
public:
	//构造函数
	MyHttpHandler();
	~MyHttpHandler();

	//向萌友请求数据，并写入数据库
	void excuteAction();

private:
	//获取异常数据列表
	int GetErrorList(string offset, string appkey, string start_date, string end_date);
	//获取登陆萌友的token
	int GetLoginToken();
	
	string host;                  //主机
	string loginPage;             //登陆网址
	string analyzePage;           //获取异常数据接口网址
	string data;                  //登陆信息
	string origin;            

	string token;                 //登陆成功时返回的token
	string errorList;             //异常数据列表，JSON格式
};