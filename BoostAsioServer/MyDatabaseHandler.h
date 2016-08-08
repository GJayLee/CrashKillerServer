#pragma once
//连接数据库
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
数据库操作类，主要是对数据库进行增删改查
*/

class MyDatabaseHandler
{
public:
	//构造函数
	MyDatabaseHandler();
	//析构函数
	~MyDatabaseHandler();
	
	//获取外部项目配置文件
	/*void GetAppkeys(std::vector<string> &res);
	void GetTables(std::vector<string> &res);
	void GetStarts(std::vector<string> &res);
	void GetEnds(std::vector<string> &res);
	void GetAppkeysTables(std::unordered_map<string, string> &res);*/

	/*
	服务端和客户端对数据库的操作
	*/
	//获取外部配置文件信息，并在数据库中创建对应的表
	void InitDatabaseTabelByAppkey();
	//把同一个人的异常信息进行初步的分类
	void AutoClassifyCrash(string tableName, string developer);
	//计算两个异常向量间的余弦相似度
	bool CalculateCos(std::vector<int> a, std::vector<int> b);

	/*
	服务端的数据库操作
	*/
	//解析从萌友获取的数据，并插入数据库中，如果数据中有该异常的信息，则不再插入
	//只插入新获取到的异常信息
	void MyDatabaseHandler::ParseJsonAndInsertToDatabase(int &count, string tableName, string errorList);
	//执行异常分类
	void MyDatabaseHandler::excuteCrashClassfy(string tableName);
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

	/*
	客户端的数据库操作
	*/
	//从数据库中获取开发者信息并转换为JSON格式
	void GetDeveloperInfo(string &developerInfo);
	//从数据库中获取数据并把数据转为JSON格式上
	void TransferDataToJson(std::vector<string> &crashInfo, string appkey);
	//从客户端收到更新信息，更新数据库
	void UpdateDatabase(string clientData);

protected:
	//外部项目配置信息
	static std::vector<string> tables;
	static std::vector<string> appkeys;
	static std::vector<string> starts;
	static std::vector<string> ends;
	static string projectConfigureInfoInJson;
	static std::unordered_map<string, string> appkey_tables;

private:
	//连接数据库
	sql::Driver *dirver;
	sql::Connection *con;
};