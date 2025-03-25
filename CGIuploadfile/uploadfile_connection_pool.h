#pragma once

#include <stdio.h>
#include <list>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../utils/utils/upload_python_files.h"
#include "../lock/locker.h"
#include "../utils/nolhmann/json.h"

using namespace std;

class uploadFileConn
{
public:
	nlohmann::json upload_file_python_server(
		const std::string &task_id, 
		const std::string &filename, 
		const std::string &fileContent, 
		const std::string &fileContentType);

	uploadFileConn();
	~uploadFileConn();

};

class uploadfile_connection_pool
{
public:
	uploadFileConn* GetConnection();				 //获取数据库连接
	bool ReleaseConnection(uploadFileConn* conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static uploadfile_connection_pool *GetInstance();

	void init(unsigned int MaxConn); 
	
	uploadfile_connection_pool();
	~uploadfile_connection_pool();

private:
	unsigned int MaxConn;  //最大连接数
	unsigned int CurConn;  //当前已使用的连接数
	unsigned int FreeConn; //当前空闲的连接数

private:
	locker lock;
	list<uploadFileConn* > connList; //连接池
	sem reserve;

private:
	
};

class uploadfile_connectionRAII{

public:
	uploadfile_connectionRAII(uploadFileConn **con, uploadfile_connection_pool *connPool);
	~uploadfile_connectionRAII();
	
private:
	uploadFileConn* conRAII;
	uploadfile_connection_pool *poolRAII;
};

