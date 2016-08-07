#pragma once

/*
声明一些通用的方法，
方便测试和使用
*/

#include <iostream>
#include <vector>
#include <fstream>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>

//UTF-8转GB2312，测试
char* U2G(const char* utf8);

//GB2312转UTF-8，测试
char* G2U(const char* gb2312);

//求三个数的最小值, 测试
int myMin(int a, int b, int c);
//简单字符串对比，返回相似度,测试
int Levenshtein(std::string str1, std::string str2);

//把数据写入到文件方便,测试
void writeFile(std::vector<std::string> res, const char *fileName);
void writeFile(std::string res, const char *fileName);
void writeFile(const char *src, const char *fileName);


//计算两个异常向量间的余弦相似度
bool CalculateCos(std::vector<int> a, std::vector<int> b);