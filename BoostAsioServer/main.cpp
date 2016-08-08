/*
日期：2016年7月25日-----2016年8月13日
CrashKiller服务端
主要功能：
1、从myou平台获取异常数据，并写入到数据库中
2、对异常数据进行处理
	1）异常数据的智能分配-------主要根据模块与开发者的关系进行分配
	2）异常数据的分类---------在每个人中对每个异常生成一个向量，然后计算向量的相似度
	3）接收客户端的更新数据，更新数据库中的异常数据
*/

#include "CrashKillerServer.h"

int main()
{
	io_service iosev;

	CrashKillerServer sev(iosev);

	// 开始等待连接
	sev.Accept();
	iosev.run();
	return 0;
}