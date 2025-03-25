#pragma once

#include <stdio.h>
#include <list>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../utils/nolhmann/json.h"
#include "../utils/utils/getparseresult_python.h"
#include "../lock/locker.h"

using namespace std;

class getParseResultConn
{
public:
	nlohmann::json get_parse_result_python_server(const std::string &task_id);

	getParseResultConn();
	~getParseResultConn();

};

class getparseresult_connection_pool
{
public:
	getParseResultConn* GetConnection();				 //获取数据库连接
	bool ReleaseConnection(getParseResultConn* conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static getparseresult_connection_pool *GetInstance();

	void init(unsigned int MaxConn); 
	
	getparseresult_connection_pool();
	~getparseresult_connection_pool();

private:
	unsigned int MaxConn;  //最大连接数
	unsigned int CurConn;  //当前已使用的连接数
	unsigned int FreeConn; //当前空闲的连接数

private:
	locker lock;
	list<getParseResultConn* > connList; //连接池
	sem reserve;

private:
	
};

class getparseresult_connectionRAII{

public:
	getparseresult_connectionRAII(getParseResultConn **con, getparseresult_connection_pool *connPool);
	~getparseresult_connectionRAII();
	
private:
	getParseResultConn* conRAII;
	getparseresult_connection_pool *poolRAII;
};

