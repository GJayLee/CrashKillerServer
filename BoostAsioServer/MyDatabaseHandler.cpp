#include "MyDatabaseHandler.h"
#include "generic.h"

using namespace boost::property_tree;

/*
���ݿ⴦����Ӧ��
*/

MyDatabaseHandler::MyDatabaseHandler()
{
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
MyDatabaseHandler::~MyDatabaseHandler()
{
	delete con;
}

/*
����˺Ϳͻ��˶����ݿ�Ĳ���
*/
//��ȡ�ⲿ�����ļ���Ϣ���������ݿ��д�����Ӧ�ı�
void MyDatabaseHandler::InitDatabaseTabelByAppkey()
{
	//��ȡ�����ļ���appkey��Ϣ
	std::ifstream t("ProjectsConfigure.txt");
	if (!t)
	{
		std::cout << "����ĿAppkey�����ļ�ʧ�ܣ�" << std::endl;
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
		//����appkey��������ݿ��в����ڸ�appkey�ı��򴴽�
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
		//��ȡ�ⲿ�����ļ��е���Ŀ��Ϣ
		projectConfigureInfoInJson = buffer.str();
	}
	catch (std::exception& e)
	{
		std::cout << "���������ļ�����" << std::endl;
	}
}
//��ͬһ���˵��쳣��Ϣ���г����ķ���
void MyDatabaseHandler::AutoClassifyCrash(string tableName, string developer)
{
	sql::PreparedStatement *pstmt;
	string sqlStatement = "select crash_id, crash_context from " + tableName + " where developer = \"" + developer + "\"";
	pstmt = con->prepareStatement(sqlStatement);
	sql::ResultSet *res;
	res = pstmt->executeQuery();

	//�쳣id�ļ���
	std::vector<string> crash_id;
	std::unordered_map<string, bool> hasClassy;
	//ÿ���쳣������Щģ��
	std::unordered_map<string, std::unordered_set<string>> crash_modules;
	//�ÿ����߸�����Щģ��
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

	//��ʼ��ÿ���쳣��ģ������
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
	//�쳣�����ͣ������쳣�������������ƶ��ж������Ƿ���ͬһ���쳣
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

	//�����쳣�����ͣ��������������ݿ���
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
//����������������������ƶȣ��ﵽ70%���������ж�Ϊͬһ���쳣
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
����˶����ݿ�Ĳ���
*/
//���������ѻ�ȡ�����ݣ����������ݿ��У�����������и��쳣����Ϣ�����ٲ���
//ֻ�����»�ȡ�����쳣��Ϣ
void MyDatabaseHandler::ParseJsonAndInsertToDatabase(int &count, string tableName, string errorList)
{
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
		InsertDataToDataBase(tableName, pt2.get<string>("crash_id"), pt2.get<string>("fixed"),
			pt2.get<string>("app_version"), pt2.get<string>("first_crash_date_time"),
			pt2.get<string>("last_crash_date_time"), pt2.get<string>("crash_context_digest"), pt2.get<string>("crash_context"));

		count++;
		//����
		//InsertModulesInfo(pt2.get<string>("crash_context"));
		//InsertDeveloperInfo(p1.get<string>("crash_context_digest"));
	}

	con->commit();
}
//ִ�в�����䣬�������в����쳣������Ϣ
void MyDatabaseHandler::InsertDataToDataBase(string tableName, const string crash_id
	, const string fixed, const string app_version, const string first_crash_date_time
	, const string last_crash_date_time, const string crash_context_digest, const string crash_context)
{
	//�����ж����ݿ����Ƿ��Ѵ��ڸ��쳣����
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
	//����������ӵ����ݿ���
	if (!isExisted)
	{
		//�����쳣��Ϣ�е�ģ����Ϣ���쳣���ܷ����������
		string developerName = AutoDistributeCrash(crash_context);

		//ʹ��replace��䣬������ݿ�������ͬ���������ݣ���������ݿ���Ϣ
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
//ִ���쳣����
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
//�����쳣��Ϣ��ȡ����ģ����Ϣ
void MyDatabaseHandler::InsertModulesInfo(string crash_context)
{
	//�ַ���ƥ��Cvte����Ϣ
	//������ʽƥ��̶���ʽ���ַ���
	//const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){1,4}";
	const char *regStr = "Cvte(\\.[0-9a-zA-Z]+){2}";
	boost::regex reg(regStr);
	boost::smatch m;
	while (boost::regex_search(crash_context, m, reg))
	{
		//����ģ����Ϣ
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
//�����쳣��Ϣ��ȡ��������Ϣ
void MyDatabaseHandler::InsertDeveloperInfo(string info)
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
//�����쳣�ж�ջ��ģ����Ϣ���ܷ����쳣�����ط���Ŀ���������
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

	return developerName;*/

	string developerName = "";
	std::unordered_map<string, int> developerWeight;
	string maxId = "";
	int maxWeight = 0;
	int count = 2;

	//�������ݿ���Ϣ
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;

	std::unordered_map<string, bool> modulesHasExisted;
	//��ȡ�쳣�����е�ģ����Ϣ
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
						//�ҳ�Ȩ�����Ŀ����ߣ���Ϊһ���쳣�п����漰����ģ�飬ÿ��ģ�鶼�ɲ�ͬ
						//������ʵ�֣�һ��ģ��������ߵ�Ȩ�ؼ�1
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
�ͻ�������ʱ�����ݿ�Ĳ���
*/
//�����ݿ��л�ȡ��������Ϣ��ת��ΪJSON��ʽ
void MyDatabaseHandler::GetDeveloperInfo(string &developerInfo)
{
	sql::Statement *stmt;
	sql::ResultSet *res;

	stmt = con->createStatement();
	developerInfo = "";
	//��developer���л�ȡ������Ϣ
	res = stmt->executeQuery("select * from developer");
	std::stringstream stream;
	ptree pt, pt2;
	//ѭ������
	while (res->next())
	{
		ptree pt1;
		//��ȡ������Ϣת��ΪJSON��ʽ
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
//�����ݿ��л�ȡ���ݲ�������תΪJSON��ʽ
void MyDatabaseHandler::TransferDataToJson(std::vector<string> &crashInfo, string appkey)
{
	//InitAppkeyTables();

	crashInfo.clear();

	sql::Statement *stmt;
	sql::ResultSet *res;

	//con->setClientOption("characterSetResults", "utf8");
	stmt = con->createStatement();

	string result = "";
	//�����ݿ���л�ȡ������Ϣ
	string sqlStatement = "select * from " + appkey_tables[appkey] + " where fixed=\"false\"";
	res = stmt->executeQuery(sqlStatement);
	//ptree pt3, pt4;
	//ѭ������
	while (res->next())
	{
		//��ȡ������Ϣת��ΪJSON��ʽ
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
	//����
	/*pt4.put_child("datas", pt3);
	std::stringstream stream;
	write_json(stream, pt4);
	writeFile(stream.str(), "dataJson.txt");*/

	//writeFile(dataInJson, "dataJson.txt");

	//����
	delete res;
	delete stmt;
}
//�ӿͻ����յ�������Ϣ���������ݿ�
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

	//�������ݿ���Ϣ
	sql::PreparedStatement *pstmt;
	sql::ResultSet *res;
	string sqlStatement;
	//���쳣�����������
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
		//���쳣��Ϣ�������·���
		AutoClassifyCrash(appkey_tables[appkey], developer);
	}
	//�쳣���Ϊ�ѽ��
	else
	{
		sql::Statement *stmt;
		stmt = con->createStatement();
		string sqlStateMent = "UPDATE " + appkey_tables[appkey] + " SET fixed = \"" + fixed + "\" WHERE crash_id = \"" + crash_id + "\"";
		stmt->execute(sqlStateMent);
		delete stmt;
	}
}