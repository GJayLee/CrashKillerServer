#pragma once

/*
����һЩͨ�õķ�����
������Ժ�ʹ��
*/

#include <iostream>
#include <vector>
#include <fstream>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\json_parser.hpp>

//UTF-8תGB2312������
char* U2G(const char* utf8);

//GB2312תUTF-8������
char* G2U(const char* gb2312);

//������������Сֵ, ����
int myMin(int a, int b, int c);
//���ַ����Աȣ��������ƶ�,����
int Levenshtein(std::string str1, std::string str2);

//������д�뵽�ļ�����,����
void writeFile(std::vector<std::string> res, const char *fileName);
void writeFile(std::string res, const char *fileName);
void writeFile(const char *src, const char *fileName);


//���������쳣��������������ƶ�
bool CalculateCos(std::vector<int> a, std::vector<int> b);