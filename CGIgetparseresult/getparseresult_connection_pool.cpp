#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "getparseresult_connection_pool.h"

using namespace std;

getParseResultConn::getParseResultConn(){}
getParseResultConn::~getParseResultConn(){}
nlohmann::json getParseResultConn::get_parse_result_python_server(const std::string &task_id)
{
	return getparsereuslt_from_pythonsever(task_id);
}

getparseresult_connection_pool::getparseresult_connection_pool()
{
	this->CurConn = 0;
	this->FreeConn = 0;
}

getparseresult_connection_pool *getparseresult_connection_pool::GetInstance()
{
	static getparseresult_connection_pool connPool;
	return &connPool;
}

//构造初始化
void getparseresult_connection_pool::init(unsigned int MaxConn)
{
	lock.lock();
	for (int i = 0; i < MaxConn; i++)
	{
		// 连接到 Rabbitmq 服务器
		getParseResultConn* channel = new getParseResultConn();
		connList.push_back(channel);
		++FreeConn;
	}
	reserve = sem(FreeConn);
	this->MaxConn = FreeConn;
	lock.unlock();
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
getParseResultConn* getparseresult_connection_pool::GetConnection()
{
	getParseResultConn* con = NULL;

	// if (0 == connList.size())
	// 	return NULL;
	reserve.wait();
	lock.lock();
	con = connList.front();
	connList.pop_front();
	--FreeConn;
	++CurConn;
	lock.unlock();
	return con;
}

//释放当前使用的连接
bool getparseresult_connection_pool::ReleaseConnection(getParseResultConn* con)
{
	if (NULL == con){
		return false;
	}
	lock.lock();
	connList.push_back(con);
	++FreeConn;
	--CurConn;
	lock.unlock();
	reserve.post();
	return true;
}

//销毁数据库连接池
void getparseresult_connection_pool::DestroyPool()
{
	lock.lock();
	if (connList.size() > 0)
	{
		CurConn = 0;
		FreeConn = 0;
		connList.clear();
	}
	lock.unlock();
}

//当前空闲的连接数
int getparseresult_connection_pool::GetFreeConn()
{
	return this->FreeConn;
}

getparseresult_connection_pool::~getparseresult_connection_pool()
{
	DestroyPool();
}

getparseresult_connectionRAII::getparseresult_connectionRAII(getParseResultConn **UPLOADCONN, getparseresult_connection_pool *connPool){
	*UPLOADCONN = connPool->GetConnection();
	
	conRAII = *UPLOADCONN;
	poolRAII = connPool;
}

getparseresult_connectionRAII::~getparseresult_connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}