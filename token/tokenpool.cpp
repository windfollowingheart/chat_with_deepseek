#include "tokenpool.h"
#include <stdio.h>
#include <list>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"


token_pool::token_pool(list<vector<string>>& tokenList)
: tokenList(tokenList)
{
	MaxToken = tokenList.size();
	FreeToken = tokenList.size();
	CurToken = 0;
	reserve = sem(FreeToken);
};

token_pool::~token_pool(){
	// for (auto& token : tokenList) {
	// 	free(token); // 释放动态分配的内存
	// }
	tokenList.clear(); // 清空列表
};

vector<string> token_pool::GetToken(){
	vector<string> token;

	if (0 == tokenList.size()){
		return token;
	}

	reserve.wait();
	
	lock.lock();

	token = tokenList.front();
	tokenList.pop_front();

	--FreeToken;
	++CurToken;

	lock.unlock();
	return token;
};				 

bool token_pool::ReleaseToken(vector<string>& token){
	if (token.size()==0){
		return false;
	}

	lock.lock();

	tokenList.push_back(token);
	++FreeToken;
	--CurToken;

	lock.unlock();

	reserve.post();
	return true;
};