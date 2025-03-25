#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include "rabbitmq_connection_pool.h"

using namespace std;

rabbitmq_connection_pool::rabbitmq_connection_pool()
{
	this->CurConn = 0;
	this->FreeConn = 0;
}

rabbitmq_connection_pool *rabbitmq_connection_pool::GetInstance()
{
	static rabbitmq_connection_pool connPool;
	return &connPool;
}

//构造初始化
void rabbitmq_connection_pool::init(string url, string User, string PassWord, int Port, unsigned int MaxConn)
{
	return;
	this->url = url;
	this->Port = Port;
	this->User = User;
	this->PassWord = PassWord;

	lock.lock();
	for (int i = 0; i < MaxConn; i++)
	{
		// 连接到 Rabbitmq 服务器
		AmqpClient::Channel::ptr_t channel = AmqpClient::Channel::Create(url, Port, User, PassWord);
		connList.push_back(channel);
		++FreeConn;
	}

	reserve = sem(FreeConn);
	this->MaxConn = FreeConn;
	
	lock.unlock();
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
AmqpClient::Channel::ptr_t rabbitmq_connection_pool::GetConnection()
{
	AmqpClient::Channel::ptr_t con = NULL;

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
bool rabbitmq_connection_pool::ReleaseConnection(AmqpClient::Channel::ptr_t con)
{
	// std::cerr << "销毁redis" << con <<std::endl;
	if (NULL == con){
		// std::cerr << "销毁redis22" << std::endl;
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
void rabbitmq_connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		// list<AmqpClient::Channel::ptr_t>::iterator it;
		// for (it = connList.begin(); it != connList.end(); ++it)
		// {
		// 	AmqpClient::Channel::ptr_t con = *it;
		// 	// 关闭通道
		// 	con->Close();
		// }
		CurConn = 0;
		FreeConn = 0;
		connList.clear();

		lock.unlock();
	}

	lock.unlock();
}

//当前空闲的连接数
int rabbitmq_connection_pool::GetFreeConn()
{
	return this->FreeConn;
}

rabbitmq_connection_pool::~rabbitmq_connection_pool()
{
	DestroyPool();
}

rabbitmq_connectionRAII::rabbitmq_connectionRAII(AmqpClient::Channel::ptr_t *RABBITMQ, rabbitmq_connection_pool *connPool){
	*RABBITMQ = connPool->GetConnection();
	
	conRAII = *RABBITMQ;
	poolRAII = connPool;
}

rabbitmq_connectionRAII::~rabbitmq_connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}