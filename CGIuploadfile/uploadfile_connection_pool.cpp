#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "uploadfile_connection_pool.h"

using namespace std;

uploadFileConn::uploadFileConn(){}
uploadFileConn::~uploadFileConn(){}
nlohmann::json uploadFileConn::upload_file_python_server(
	const std::string &task_id, 
	const std::string &filename, 
	const std::string &fileContent, 
	const std::string &fileContentType)
{
	return upload_file_to_pythonsever(task_id, filename, fileContent, fileContentType);
}

uploadfile_connection_pool::uploadfile_connection_pool()
{
	this->CurConn = 0;
	this->FreeConn = 0;
}

uploadfile_connection_pool *uploadfile_connection_pool::GetInstance()
{
	static uploadfile_connection_pool connPool;
	return &connPool;
}

//构造初始化
void uploadfile_connection_pool::init(unsigned int MaxConn)
{
	lock.lock();
	for (int i = 0; i < MaxConn; i++)
	{
		// 连接到 Rabbitmq 服务器
		uploadFileConn* channel = new uploadFileConn();
		connList.push_back(channel);
		++FreeConn;
	}
	reserve = sem(FreeConn);
	this->MaxConn = FreeConn;
	lock.unlock();
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
uploadFileConn* uploadfile_connection_pool::GetConnection()
{
	uploadFileConn* con = NULL;

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
bool uploadfile_connection_pool::ReleaseConnection(uploadFileConn* con)
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
void uploadfile_connection_pool::DestroyPool()
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
int uploadfile_connection_pool::GetFreeConn()
{
	return this->FreeConn;
}

uploadfile_connection_pool::~uploadfile_connection_pool()
{
	DestroyPool();
}

uploadfile_connectionRAII::uploadfile_connectionRAII(uploadFileConn **UPLOADCONN, uploadfile_connection_pool *connPool){
	*UPLOADCONN = connPool->GetConnection();
	
	conRAII = *UPLOADCONN;
	poolRAII = connPool;
}

uploadfile_connectionRAII::~uploadfile_connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}