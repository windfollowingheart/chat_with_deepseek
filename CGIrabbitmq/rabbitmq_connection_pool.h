#pragma once

#include <stdio.h>
#include <list>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include "../lock/locker.h"

using namespace std;

class rabbitmq_connection_pool
{
public:
	AmqpClient::Channel::ptr_t GetConnection();				 //获取数据库连接
	bool ReleaseConnection(AmqpClient::Channel::ptr_t conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static rabbitmq_connection_pool *GetInstance();

	void init(string url, string User, string PassWord, int Port, unsigned int MaxConn); 
	
	rabbitmq_connection_pool();
	~rabbitmq_connection_pool();

private:
	unsigned int MaxConn;  //最大连接数
	unsigned int CurConn;  //当前已使用的连接数
	unsigned int FreeConn; //当前空闲的连接数

private:
	locker lock;
	list<AmqpClient::Channel::ptr_t> connList; //连接池
	sem reserve;

private:
	string url;			 //主机地址
	string Port;		 //数据库端口号
	string User;		 //登陆数据库用户名
	string PassWord;	 //登陆数据库密码
};

class rabbitmq_connectionRAII{

public:
	rabbitmq_connectionRAII(AmqpClient::Channel::ptr_t *con, rabbitmq_connection_pool *connPool);
	~rabbitmq_connectionRAII();
	
private:
	AmqpClient::Channel::ptr_t conRAII;
	rabbitmq_connection_pool *poolRAII;
};

