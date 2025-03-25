#pragma once

#include <list>
#include <iostream>
#include <string>
#include <stdio.h>
#include <error.h>
#include <string.h>
#include <hiredis/hiredis.h>
#include "../lock/locker.h"

using namespace std;

class redis_connection_pool
{
public:
	redisContext *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(redisContext *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static redis_connection_pool *GetInstance();

	void init(string url, string User, string PassWord, int DataBaseIndex, int Port, unsigned int MaxConn); 
	
	redis_connection_pool();
	~redis_connection_pool();

private:
	unsigned int MaxConn;  //最大连接数
	unsigned int CurConn;  //当前已使用的连接数
	unsigned int FreeConn; //当前空闲的连接数

private:
	locker lock;
	list<redisContext *> connList; //连接池
	sem reserve;

private:
	string url;			 //主机地址
	string Port;		 //数据库端口号
	string User;		 //登陆数据库用户名
	string PassWord;	 //登陆数据库密码
	int DataBaseIndex; //使用数据库名
};

class redis_connectionRAII{

public:
	redis_connectionRAII(redisContext **con, redis_connection_pool *connPool);
	~redis_connectionRAII();
	
private:
	redisContext *conRAII;
	redis_connection_pool *poolRAII;
};

