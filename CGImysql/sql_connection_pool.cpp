#include <string>
#include <list>
#include <iostream>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
	this->CurConn = 0;
	this->FreeConn = 0;
}

connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn)
{
	this->url = url;
	this->Port = Port;
	this->User = User;
	this->PassWord = PassWord;
	this->DatabaseName = DBName;

	lock.lock();
	for (int i = 0; i < MaxConn; i++)
	{
		
		// con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
		MYSQL *con = get_conn();

		if (con == NULL)
		{
			cout << "Error: " << mysql_error(con);
			exit(1);
		}
		connList.push_back(con);
		++FreeConn;
	}



	reserve = sem(FreeConn);

	this->MaxConn = FreeConn;
	
	lock.unlock();
}

MYSQL* connection_pool::get_conn(){
	
	MYSQL *con = mysql_init(NULL);

	if (con == NULL)
	{
		cout << "Error:" << mysql_error(con);
		exit(1);
	}
	cout << "START: " << Port <<endl;
	return mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DatabaseName.c_str(), Port, NULL, 0);
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;

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
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	// std::cout << "将mysql重新加入连接池: " << con << std::endl;
	if (NULL == con)
	{
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
void connection_pool::DestroyPool()
{
	std::cout << "开始销毁mysql连接池: " << connList.size() << std::endl;
	lock.lock();
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			std::cout << "开始关闭了一个mysql: "  << std::endl;
			MYSQL *con = *it;
			std::cout << "开始关闭了一个mysql: " << (con==NULL) << "  " << con << std::endl;
			mysql_close(con);
			std::cout << "关闭了一个mysql" << std::endl;
		}
		CurConn = 0;
		FreeConn = 0;
		connList.clear();

		lock.unlock();
	}
	std::cout << "完成销毁mysql连接池" << std::endl;
	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->FreeConn;
}

connection_pool::~connection_pool()
{
	DestroyPool();
}

connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	
	conRAII = *SQL; // 这里是解引用
	poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
	// std::cout << "mysql析构了: " << (conRAII == NULL) << "  " << conRAII << std::endl;
	poolRAII->ReleaseConnection(conRAII);
}

void connectionRAII::updateConRAII(MYSQL **con)
{
	conRAII = *con;
}