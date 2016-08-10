#include "MyDatabaseHandler.h"
#include "generic.h"

using namespace boost::property_tree;

/*
数据库处理响应类
*/

MyDatabaseHandler::MyDatabaseHandler()
{
	//连接数据库
	dirver = get_driver_instance();
	con = dirver->connect("localhost", "root", "123456");
	if (con == NULL)
	{
		std::cout << "不能连接数据库！" << std::endl;
		return;
	}
	//选择CrashKiller数据库
	con->setSchema("CrashKiller");
}
MyDatabaseHandler::~MyDatabaseHandler()
{
	delete con;
}

/*
服务端和客户端对数据库的操作
*/
//获取外部配置文件信息，并在数据库中创建对应的表
void MyDatabaseHandler::InitDatabaseTabelByAppkey()
{
	//读取配置文件中appkey信息
	std::ifstream t("ProjectsConfigure.txt");
	if (!t)
	{
		std::cout << "打开项目Appkey配置文件失败！" << std::endl;
		return;
	}
	std::stringstream buffer;
	buffer << t.rdbuf();

	try
	{
		ptree pt, pt1, pt2;
		read_json<ptree>(buffer, pt);
		pt1 = pt.get_child("projects");
		for (ptree::iterator it = pt1.begin(); it != pt1.end(); ++it)
		{
			pt2 = it->second;
			appkeys.push_back(pt2.get<string>("appkey"));
			tables.push_back(pt2.get<string>("tableName"));
			starts.push_back(pt2.get<string>("start_date"));
			ends.push_back(pt2.get<string>("end_date"));
			appkey_tables[pt2.get<string>("appkey")] = pt2.get<string>("tableName");
		}

		con->setAutoCommit(0);
		//遍历appkey，如果数据库中不存在该appkey的表，则创建
		for (int i = 0; i < appkeys.size(); i++)
		{
			sql::PreparedStatement *pstmt;
			string sqlStatement;
			string str1 = "CREATE TABLE if not EXISTS " + tables[i] + " (crash_id varchar(255) PRIMARY KEY NOT NULL,";
			string str2 = "developer varchar(255) DEFAULT NULL, fixed VARCHAR(255) DEFAULT NULL,crash_type VARCHAR(255) DEFAULT NULL,app_version VARCHAR(255) DEFAULT NULL,";
			string str3 = "first_crash_date_time VARCHAR(255) DEFAULT NULL,last_crash_date_time VARCHAR(255) DEFAULT NULL,";
			string str4 = "crash_context_digest VARCHAR(255) DEFAULT NULL,crash_context text DEFAULT NULL) ENGINE = InnoDB DEFAULT CHARSET = utf8";
			sqlStatement = str1 + str2 + str3 + str4;
			pstmt = con->prepareStatement(sqlStatement);
			pstmt->execute();

			delete pstmt;
		}
		con->commit();
		//获取外部配置文件中的项目信息
		projectConfigureInfoInJson = buffer.str();
	}
	catch (std::exception& e)
	{
		std::cout << "解析配置文件出错！" << std::endl;
	}
}
//把同一个人的异常信息进行初步的分类
void MyDatabaseHandler::AutoClassifyCrash(string tableName, string developer)
{
	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_id, crash_context from " + tableName + " where developer = \"" + developer + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();

	//异常id的集合
	std::vector<string> crash_id;
	std::unordered_map<string, bool> hasClassy;
	//每个异常中有哪些模块
	std::unordered_map<string, std::unordered_set<string>> crash_modules;
	//该开发者负责哪些模块
	std::unordered_set<string> crash_totalModules;

	while (res->next())
	{
		crash_id.push_back(res->getString("crash_id"));
		hasClassy[res->getString("crash_id")] = false;
		const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2,9}";
		boost::regex reg(regStr);
		boost::smatch m;
		string crash_context = res->getString("crash_context");
		while (boost::regex_search(crash_context, m, reg))
		{
			boost::smatch::iterator it = m.begin();
			crash_totalModules.insert(it->str());
			crash_modules[res->getString("crash_id")].insert(it->str());
			crash_context = m.suffix().str();
		}
	}

	delete pstmt;
	delete res;

	//初始化每个异常的模块向量
	std::unordered_map<string, std::vector<int>> crash_vectors;
	std::unordered_set<string>::iterator iterTotalModules = crash_totalModules.begin();
	for (; iterTotalModules != crash_totalModules.end(); ++iterTotalModules)
	{
		std::unordered_map<string, std::unordered_set<string>>::iterator iter = crash_modules.begin();
		for (; iter != crash_modules.end(); ++iter)
		{
			if (iter->second.find((*iterTotalModules)) == iter->second.end())
				crash_vectors[iter->first].push_back(0);
			else
				crash_vectors[iter->first].push_back(1);
		}
	}
	//异常的类型，根据异常向量的余弦相似度判断它们是否是同一个异常
	int type = 0;
	std::unordered_map<int, std::vector<string>> crash_types;
	std::unordered_map<string, std::vector<int>>::iterator iter = crash_vectors.begin();
	for (int i = 0; i < crash_id.size(); i++)
	{
		if (!hasClassy[crash_id[i]])
		{
			crash_types[type].push_back(crash_id[i]);
			if (crash_vectors.find(crash_id[i]) == crash_vectors.end())
				continue;
		}
		else
			continue;
		for (int j = i + 1; j < crash_id.size(); j++)
		{
			if (crash_vectors.find(crash_id[j]) == crash_vectors.end())
				continue;
			if (CalculateCos(crash_vectors[crash_id[i]], crash_vectors[crash_id[j]]))
			{
				crash_types[type].push_back(crash_id[j]);
				hasClassy[crash_id[j]] = true;
			}
		}
		++type;
	}

	//遍历异常的类型，并把它插入数据库中
	con->setAutoCommit(0);
	for (int i = 0; i < type; i++)
	{
		for (int j = 0; j < crash_types[i].size(); j++)
		{
			std::stringstream ss;
			ss << i;
			sqlStatement = "update " + tableName + " set crash_type=\"" + ss.str() + "\" where crash_id = \"" + crash_types[i][j] + "\"";
			pstmt = con->prepareStatement(sqlStatement);
			pstmt->execute();
			delete pstmt;
		}
	}
	con->commit();
}
//计算两个向量间的余弦相似度，达到70%的相似则判断为同一类异常
bool MyDatabaseHandler::CalculateCos(std::vector<int> a, std::vector<int> b)
{
	float lengthA = 0, lengthB = 0;
	float dotProduct = 0;
	for (int i = 0; i < a.size(); i++)
	{
		lengthA += (float)pow(a[i], 2);
		lengthB += (float)pow(b[i], 2);
		dotProduct += (a[i] * b[i]);
	}
	lengthA = sqrt(lengthA);
	lengthB = sqrt(lengthB);

	int similarity = (int)(100 * dotProduct / (lengthA*lengthB));
	return similarity >= 70 ? true : false;
}

/*
服务端对数据库的操作
*/
//解析从萌友获取的数据，并插入数据库中，如果数据中有该异常的信息，则不再插入
//只插入新获取到的异常信息
void MyDatabaseHandler::ParseJsonAndInsertToDatabase(int &count, string tableName, string errorList)
{
	//统计该请求是否已达到最大值，limit
	count = 0;
	//如果获取的异常数列为空，则证明已接收完成
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
		InsertDataToDataBase(tableName, pt2.get<string>("crash_id"), pt2.get<string>("fixed"),
			pt2.get<string>("app_version"), pt2.get<string>("first_crash_date_time"),
			pt2.get<string>("last_crash_date_time"), pt2.get<string>("crash_context_digest"), pt2.get<string>("crash_context"));

		count++;
		//测试
		//InsertModulesInfo(pt2.get<string>("crash_context"));
		//InsertDeveloperInfo(p1.get<string>("crash_context_digest"));
	}

	con->commit();
}
//执行插入语句，向数据中插入异常数据信息
void MyDatabaseHandler::InsertDataToDataBase(string tableName, const string crash_id
	, const string fixed, const string app_version, const string first_crash_date_time
	, const string last_crash_date_time, const string crash_context_digest, const string crash_context)
{
	//首先判断数据库中是否已存在该异常数据
	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_context from " + tableName + " where crash_id = \"" + crash_id + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();
	bool isExisted = false;
	while (res->next())
		isExisted = true;

	delete pstmt;
	delete res;
	//不存在则添加到数据库中
	if (!isExisted)
	{
		//根据异常信息中的模块信息把异常智能分配给开发者
		string developerName = AutoDistributeCrash(crash_context);

		//使用replace语句，如果数据库中有相同主键的数据，则更新数据库信息
		//pstmt = con->prepareStatement("REPLACE INTO ErrorInfo(crash_id,developerid,fixed,app_version,first_crash_date_time,last_crash_date_time,crash_context_digest,crash_context) VALUES(?,?,?,?,?,?,?,?)");
		sqlStatement = "INSERT INTO " + tableName
			+ "(crash_id,developer,fixed,app_version,first_crash_date_time,last_crash_date_time,crash_context_digest,crash_context) VALUES(?,?,?,?,?,?,?,?)";
		pstmt = con->prepareStatement(sqlStatement);

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
//执行异常分类
void MyDatabaseHandler::excuteCrashClassfy(string tableName)
{
	sql::PreparedStatement *pstmt;
	string sqlStatement = "select developer from " + tableName + " group by developer";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();
	while (res->next())
	{
		AutoClassifyCrash(tableName, res->getString("developer"));
	}
	delete pstmt;
	delete res;
}
//根据异常信息提取功能模块信息
void MyDatabaseHandler::InsertModulesInfo(string crash_context)
{
	//字符串匹配Cvte的信息
	//正则表达式匹配固定格式的字符串
	//const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){1,4}";
	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
	boost::regex reg(regStr);
	boost::smatch m;
	while (boost::regex_search(crash_context, m, reg))
	{
		//遍历模块信息
		boost::smatch::iterator it = m.begin();
		//std::cout << it->str() << std::endl;
		sql::PreparedStatement *pstmt;
		//插入模块信息，如果数据库中有相同主键的数据，则更新数据库信息
		pstmt = con->prepareStatement("INSERT IGNORE INTO moduleInfo(module_name) VALUES(?)");
		pstmt->setString(1, it->str());
		pstmt->execute();
		delete pstmt;

		crash_context = m.suffix().str();
	}
}
//根据异常信息提取发现者信息
void MyDatabaseHandler::InsertDeveloperInfo(string info)
{
	//第一次出现等号的位置
	int equalIndex = info.find_first_of("=");
	//第一次出现左括号的位置
	int leftIndex = info.find_first_of("(");
	//第一次出现右括号的位置
	int rightIndex = info.find_first_of(")");
	//开发者id的长度
	int idLength = leftIndex - 1 - equalIndex;
	string id;
	if (idLength > 0)
		id = info.substr(equalIndex + 1, idLength);
	else
		return;
	//开发者名字的长度
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
//根据异常中堆栈的模块信息智能分配异常，返回分配的开发者名字
string MyDatabaseHandler::AutoDistributeCrash(string crash_context)
{
	/*string developerName = "";
	string moduleName = "";
	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
	boost::regex reg(regStr);
	boost::smatch m;
	std::unordered_map<string, int> developerWeight;
	int maxWeight = 0;
	string maxId;

	//查找数据库信息
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;

	while (boost::regex_search(crash_context, m, reg))
	{
		boost::smatch::iterator it = m.begin();
		moduleName = it->str();

		sqlStatement = "select developer_id from moduleinfo where module_name = \"" + moduleName + "\"";
		//从errorinfo表中获取所有信息
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
			//找出权重最大的开发者，因为一条异常中可能涉及几个模块，每个模块都由不同
			//开发者实现，一个模块给开发者的权重加1
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

	return developerName;*/

	string developerName = "";
	std::unordered_map<string, int> developerWeight;
	string maxId = "";
	int maxWeight = 0;
	int count = 2;

	//查找数据库信息
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;

	std::unordered_map<string, bool> modulesHasExisted;
	//提取异常数据中的模块信息
	for (; count <= 4; count++)
	{
		string context = crash_context;
		string moduleName = "";
		std::stringstream ss;
		ss << count;
		string temp = "Cvte(\\.[0-9a-zA-Z]+){" + ss.str() + "}";
		const char *regStr = temp.c_str();
		boost::regex reg(regStr);
		boost::smatch m;

		while (boost::regex_search(context, m, reg))
		{
			boost::smatch::iterator it = m.begin();
			moduleName = it->str();
			if (modulesHasExisted.find(moduleName) == modulesHasExisted.end())
			{
				modulesHasExisted[moduleName] = true;
				sqlStatement = "select developer_id from moduleinfo where module_name = \"" + moduleName + "\"";
				pstmt = con->prepareStatement(sqlStatement);
				res = pstmt->executeQuery();

				string developer_id = "";
				while (res->next())
					developer_id = res->getString("developer_id");

				delete pstmt;
				delete res;

				if (developer_id != "")
				{
					if (developerWeight.find(developer_id) == developerWeight.end())
					{
						developerWeight[developer_id] = 1;
						maxId = developer_id;
					}
					else
					{
						//找出权重最大的开发者，因为一条异常中可能涉及几个模块，每个模块都由不同
						//开发者实现，一个模块给开发者的权重加1
						developerWeight[developer_id]++;
						if (developerWeight[developer_id] > maxWeight)
						{
							maxId = developer_id;
							maxWeight = developerWeight[developer_id];
						}
					}
				}
			}
			
			/*sqlStatement = "select module_name from modules where module_name = \"" + moduleName + "\"";
			pstmt = con->prepareStatement(sqlStatement);
			res = pstmt->executeQuery();
			bool hasExisted = false;
			while (res->next())
				hasExisted = true;

			delete pstmt;
			delete res;*/
			
			/*sqlStatement = "insert ignore into modules (module_name) values(?)";
			pstmt = con->prepareStatement(sqlStatement);
			pstmt->setString(1, moduleName);
			pstmt->execute();
			delete pstmt;*/

			context = m.suffix().str();
		}
	}

	sqlStatement = "select Name from developer where ID = \"" + maxId + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	res = pstmt->executeQuery();

	while (res->next())
		developerName = res->getString("Name");

	delete pstmt;
	delete res;

	return developerName;
}

/*
客户端请求时对数据库的操作
*/
//从数据库中获取开发者信息并转换为JSON格式
void MyDatabaseHandler::GetDeveloperInfo(string &developerInfo)
{
	sql::Statement *stmt;
	sql::ResultSet *res;

	stmt = con->createStatement();
	developerInfo = "";
	//从developer表中获取所有信息
	res = stmt->executeQuery("select * from developer");
	std::stringstream stream;
	ptree pt, pt2;
	//循环遍历
	while (res->next())
	{
		ptree pt1;
		//把取出的信息转换为JSON格式
		pt1.put("Id", res->getString("ID"));
		pt1.put("Name", res->getString("Name"));
		pt2.push_back(make_pair("", pt1));
	}

	pt.put_child("developer", pt2);
	write_json(stream, pt);
	developerInfo = stream.str();

	writeFile(developerInfo, "developInfo.txt");

	delete stmt;
	delete res;
}
//从数据库中获取数据并把数据转为JSON格式
void MyDatabaseHandler::TransferDataToJson(std::vector<string> &crashInfo, string appkey)
{
	//InitAppkeyTables();

	crashInfo.clear();

	sql::Statement *stmt;
	sql::ResultSet *res;

	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";
	//从数据库表中获取所有信息
	string sqlStatement = "select * from " + appkey_tables[appkey] + " where fixed=\"false\"";
	res = stmt->executeQuery(sqlStatement);
	//ptree pt3, pt4;
	//循环遍历
	while (res->next())
	{
		//把取出的信息转换为JSON格式
		std::stringstream stream;
		ptree pt, pt1, pt2;
		pt1.put("crash_id", res->getString("crash_id"));
		pt1.put("developer", res->getString("developer"));
		pt1.put("fixed", res->getString("fixed"));
		pt1.put("crash_type", res->getString("crash_type"));
		pt1.put("app_version", res->getString("app_version"));
		pt1.put("first_crash_date_time", res->getString("first_crash_date_time"));
		pt1.put("last_crash_date_time", res->getString("last_crash_date_time"));
		pt1.put("crash_context_digest", res->getString("crash_context_digest"));
		pt1.put("crash_context", res->getString("crash_context"));

		pt2.push_back(make_pair("", pt1));
		//pt3.push_back(make_pair("", pt1));
		pt.put_child("data", pt2);
		write_json(stream, pt);
		crashInfo.push_back(stream.str());
	}
	//测试
	/*pt4.put_child("datas", pt3);
	std::stringstream stream;
	write_json(stream, pt4);
	writeFile(stream.str(), "dataJson.txt");*/

	//writeFile(dataInJson, "dataJson.txt");

	//清理
	delete res;
	delete stmt;
}
//从客户端收到更新信息，更新数据库
void MyDatabaseHandler::UpdateDatabase(string clientData)
{
	string crash_id, developerId, fixed, appkey;

	ptree pt;
	std::stringstream stream;
	stream << clientData;
	read_json<ptree>(stream, pt);
	developerId = pt.get<string>("Developer");
	crash_id = pt.get<string>("CrashId");
	fixed = pt.get<string>("HasSolve");
	appkey = pt.get<string>("Appkey");

	//查找数据库信息
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;
	//把异常分配给开发者
	if (developerId != "null")
	{
		string developer;
		sqlStatement = "select Name from developer where ID = \"" + developerId + "\"";
		pstmt = con->prepareStatement(sqlStatement);
		res = pstmt->executeQuery();
		while (res->next())
			developer = res->getString("Name");

		sql::Statement *stmt;
		stmt = con->createStatement();
		string sqlStateMent = "UPDATE " + appkey_tables[appkey] + " SET developer = \"" + developer
			+ "\",fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);

		delete stmt;
		delete pstmt;
		delete res;
		//对异常信息进行重新分类
		AutoClassifyCrash(appkey_tables[appkey], developer);
	}
	//异常标记为已解决
	else
	{
		sql::Statement *stmt;
		stmt = con->createStatement();
		string sqlStateMent = "UPDATE " + appkey_tables[appkey] + " SET fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);
		delete stmt;
	}
}