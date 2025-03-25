#include <string>
#include <iostream>
#include <list>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <hiredis/hiredis.h>
#include "redis_connection_pool.h"

using namespace std;

redis_connection_pool::redis_connection_pool()
{
	this->CurConn = 0;
	this->FreeConn = 0;
}

redis_connection_pool *redis_connection_pool::GetInstance()
{
	static redis_connection_pool connPool;
	return &connPool;
}

//构造初始化
void redis_connection_pool::init(string url, string User, string PassWord, int DBIndex, int Port, unsigned int MaxConn)
{
	this->url = url;
	this->Port = Port;
	this->User = User;
	this->PassWord = PassWord;
	this->DataBaseIndex = DBIndex;

	lock.lock();
	for (int i = 0; i < MaxConn; i++)
	{
		// 连接到 Redis 服务器
		redisContext *context = redisConnect(url.c_str(), Port);
		if (context == nullptr || context->err) {
			if (context) {
				std::cerr << "Error: " << context->errstr << std::endl;
				redisFree(context);
			} else {
				std::cerr << "Can't allocate redis context" << std::endl;
			}
			// return ;
		}

		
		connList.push_back(context);
		++FreeConn;
	}

	reserve = sem(FreeConn);

	this->MaxConn = FreeConn;
	
	lock.unlock();
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
redisContext *redis_connection_pool::GetConnection()
{
	redisContext *con = NULL;

	// if (0 == connList.size())
	// 	return NULL;

	reserve.wait();
	
	lock.lock();

	con = connList.front();
	connList.pop_front();
	// std::cerr << "获取一个redis"  <<std::endl;
	--FreeConn;
	++CurConn;
	// std::cerr << FreeConn  <<std::endl;

	lock.unlock();
	return con;
}

//释放当前使用的连接
bool redis_connection_pool::ReleaseConnection(redisContext *con)
{
	// std::cerr << "销毁redis"  <<std::endl;
	if (NULL == con){
		// std::cerr << "销毁redis22" << std::endl;
		return false;
	}

	lock.lock();
	// std::cerr << "销毁redis22"  <<std::endl;
	connList.push_back(con);
	++FreeConn;
	--CurConn;
	// std::cerr << "销毁redis33"  <<std::endl;
	// std::cerr << FreeConn  <<std::endl;
	lock.unlock();
	
	reserve.post();
	return true;
}

//销毁数据库连接池
void redis_connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		list<redisContext *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			redisContext *con = *it;
			redisFree(con);
		}
		CurConn = 0;
		FreeConn = 0;
		connList.clear();

		lock.unlock();
	}

	lock.unlock();
}

//当前空闲的连接数
int redis_connection_pool::GetFreeConn()
{
	return this->FreeConn;
}

redis_connection_pool::~redis_connection_pool()
{
	DestroyPool();
}

redis_connectionRAII::redis_connectionRAII(redisContext **REDIS, redis_connection_pool *connPool){
	*REDIS = connPool->GetConnection();
	
	conRAII = *REDIS;
	poolRAII = connPool;
}

redis_connectionRAII::~redis_connectionRAII(){
	// std::cerr << "通过析构函数销毁redis33"  <<std::endl;
	poolRAII->ReleaseConnection(conRAII);
}