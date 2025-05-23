#ifndef _TOKEN_POOL_
#define _TOKEN_POOL_

#include <stdio.h>
#include <list>
#include <string.h>
#include <vector>
#include <string>
#include <iostream>
#include <string>
#include "../lock/locker.h"

using namespace std;

class token_pool
{
public:
	token_pool(list<vector<string>>& tokenList);
	~token_pool();
	vector<string> GetToken();
	bool ReleaseToken(vector<string>& token);
	

private:
	unsigned int MaxToken;  //最大连接数
	unsigned int CurToken;  //当前已使用的连接数
	unsigned int FreeToken; //当前空闲的连接数

private:
	locker lock;
	list<vector<string>> tokenList; //连接池
	sem reserve;

};


#endif
