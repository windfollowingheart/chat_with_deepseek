#pragma once

#include <list>
#include <iostream>
#include <string>
#include <stdio.h>
#include <error.h>
#include <string.h>
#include <mysql/mysql.h>
#include "../lock/locker.h"

using namespace std;

class connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	MYSQL *get_conn(); // 创建连接

	//单例模式
	static connection_pool *GetInstance();

	void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn); 
	
	connection_pool();
	~connection_pool();

private:
	unsigned int MaxConn;  //最大连接数
	unsigned int CurConn;  //当前已使用的连接数
	unsigned int FreeConn; //当前空闲的连接数

private:
	locker lock;
	list<MYSQL *> connList; //连接池
	sem reserve;

private:
	string url;			 //主机地址
	int Port;		 //数据库端口号
	string User;		 //登陆数据库用户名
	string PassWord;	 //登陆数据库密码
	string DatabaseName; //使用数据库名
};

class connectionRAII{

public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();

	void updateConRAII(MYSQL **con);
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

