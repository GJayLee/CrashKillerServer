#pragma once

//连接数据库
#include<cppconn\driver.h>
#include<cppconn\exception.h>
#include <cppconn/resultset.h> 
#include <cppconn/statement.h>
#include<mysql_connection.h>
#include <cppconn/prepared_statement.h>

using std::string;

/*从萌友平台获取数据，HTTP请求处理类,以及数据库的操作
把获取到的数据写入数据库中*/
class MyHttpHandler
{
public:
	//构造函数
	MyHttpHandler();
	MyHttpHandler(string a, string s, string e);
	~MyHttpHandler();

	//设置项目ID
	void setAppKey(string key);
	//设置异常数据的开始日期
	void setStartDate(string date);
	//设置异常数据的结束日期
	void setEndDate(string date);
	//向萌友请求数据，并写入数据库
	void excuteAction();
	//把同一个人的异常信息进行初步的分类
	void AutoClassifyCrash(string developer);

private:
	//获取异常数据列表
	int GetErrorList(string offset, string appkey, string start_date, string end_date);
	//获取登陆萌友的token
	int GetLoginToken();
	//解析异常数据列表，并写入数据库中
	void ParseJsonAndInsertToDatabase(string tableName);

	//根据外部配置文件中的appkey在数据库中创建对应的表
	void InitDatabaseTabelByAppkey();
	//执行插入语句，向数据中插入异常数据信息
	void InsertDataToDataBase(string tableName, const string crash_id
		, const string fixed, const string app_version, const string first_crash_date_time
		, const string last_crash_date_time, const string crash_context_digest, const string crash_context);
	//根据异常信息提取功能模块信息
	void InsertModulesInfo(string crash_context);
	//根据异常信息提取发现者信息
	void InsertDeveloperInfo(string info);
	//根据异常中堆栈的模块信息智能分配异常，返回分配的开发者名字
	string AutoDistributeCrash(string crash_context);
	//把同一个人的异常信息进行初步的分类
	//void AutoClassifyCrash(string developer);
	
	//简单字符串对比，返回相似度
	int Levenshtein(string str1, string str2);
	//求三个数的最小值
	int min(int a, int b, int c);

	//把数据写入到文本文件中方便查看，测试
	void writeFile(const char *src, const char *fileName);

	string host;                  //主机
	string loginPage;             //登陆网址
	string analyzePage;           //获取异常数据接口网址
	string data;                  //登陆信息
	string origin;            
	//string appkey;                //项目ID
	//string start_date;            //异常数据的开始日期
	//string end_date;              //异常数据的结束日期

	std::vector<string> tables;
	std::vector<string> appkeys;
	std::vector<string> starts;
	std::vector<string> ends;

	string token;                 //登陆成功时返回的token
	string errorList;             //异常数据列表，JSON格式
								  
	int count;                    //统计该请求是否已达到最大值，limit

	//连接数据库
	sql::Driver *dirver;
	sql::Connection *con;
};