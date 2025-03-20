#include "tokenpool.h"
#include <stdio.h>
#include <list>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"


token_pool::token_pool(list<char *> tokenList)
: tokenList(tokenList)
{
	MaxToken = tokenList.size();
	FreeToken = tokenList.size();
	CurToken = 0;
	reserve = sem(FreeToken);
};

token_pool::~token_pool(){
	for (auto& token : tokenList) {
		free(token); // 释放动态分配的内存
	}
	tokenList.clear(); // 清空列表
};

char* token_pool::GetToken(){
	char *token = NULL;

	if (0 == tokenList.size()){
		return NULL;
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

bool token_pool::ReleaseToken(char *token){
	if (NULL == token){
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