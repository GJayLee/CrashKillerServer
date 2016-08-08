
/*
通用函数的定义
*/

#include <Windows.h>

#include "generic.h"

//UTF-8转GB2312，测试
char* U2G(const char* utf8)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	memset(wstr, 0, len + 1);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, len);
	len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* str = new char[len + 1];
	memset(str, 0, len + 1);
	WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, len, NULL, NULL);
	if (wstr) delete[] wstr;
	return str;
}

//GB2312转UTF-8，测试
char* G2U(const char* gb2312)
{
	int len = MultiByteToWideChar(CP_ACP, 0, gb2312, -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	memset(wstr, 0, len + 1);
	MultiByteToWideChar(CP_ACP, 0, gb2312, -1, wstr, len);
	len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* str = new char[len + 1];
	memset(str, 0, len + 1);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
	if (wstr) delete[] wstr;
	return str;
}

//求三个数的最小值, 测试
int myMin(int a, int b, int c)
{
	if (a > b) {
		if (b > c)
			return c;
		else
			return b;
	}
	if (a > c) {
		if (c > b)
			return b;
		else
			return c;
	}
	if (b > c) {
		if (c > a)
			return a;
		else
			return c;
	}
}
//简单字符串对比，返回相似度,测试
int Levenshtein(std::string str1, std::string str2)
{
	int len1 = str1.length();
	int len2 = str2.length();
	int **dif = new int*[len1 + 1];
	for (int i = 0; i < len1 + 1; i++)
	{
		dif[i] = new int[len2 + 1];
		memset(dif[i], 0, sizeof(int)*(len2 + 1));
	}
	for (int i = 0; i <= len1; i++)
		dif[i][0] = i;
	for (int i = 0; i <= len2; i++)
		dif[0][i] = i;

	for (int i = 1; i <= len1; i++)
	{
		for (int j = 1; j <= len2; j++)
		{
			if (str1[i - 1] == str2[j - 1])
				dif[i][j] = dif[i - 1][j - 1];
			else
				dif[i][j] = myMin(dif[i][j - 1], dif[i - 1][j], dif[i - 1][j - 1]) + 1;
		}
	}
	float similarity = 1 - (float)dif[len1][len2] / max(len1, len2);
	//百分比
	similarity *= 100;

	for (int i = 0; i < len1 + 1; i++)
		delete[] dif[i];

	return (int)similarity;
}

//把数据写入到文件方便,测试
void writeFile(std::vector<std::string> res, const char *fileName)
{
	std::ofstream out(fileName, std::ios::out);
	if (!out)
	{
		std::cout << "Can not Open this file!" << std::endl;
		return;
	}

	std::stringstream stream;
	boost::property_tree::ptree pt, pt1;
	for (int i = 0; i < res.size(); i++)
	{
		/*out << res[i];
		out << std::endl;*/
		//ptree pt1;
		pt1.put("", res[i]);

		pt.push_back(make_pair("", pt1));
	}
	//pt.put_child("datas", pt2);
	write_json(stream, pt);

	out << stream.str();

	out.close();
}
void writeFile(std::string res, const char *fileName)
{
	std::ofstream out(fileName, std::ios::out);
	if (!out)
	{
		std::cout << "Can not Open this file!" << std::endl;
		return;
	}
	out << res;
	out << std::endl;

	out.close();
}
void writeFile(const char *src, const char *fileName)
{
	std::ofstream out(fileName, std::ios::out);
	if (!out)
	{
		std::cout << "Can not Open this file!" << std::endl;
		return;
	}
	out << src;
	out << std::endl;

	out.close();
}


//计算两个向量间的余弦相似度
bool CalculateCos(std::vector<int> a, std::vector<int> b)
{
	float lengthA = 0, lengthB = 0;
	float dotProduct = 0;
	for (int i = 0; i < a.size(); i++)
	{
		lengthA += pow(a[i], 2);
		lengthB += pow(b[i], 2);
		dotProduct += (a[i] * b[i]);
	}
	lengthA = sqrt(lengthA);
	lengthB = sqrt(lengthB);

	int similarity = 100 * dotProduct / (lengthA*lengthB);
	return similarity >= 70 ? true : false;
}