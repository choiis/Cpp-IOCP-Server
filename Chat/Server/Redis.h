
#ifndef BUSINESSSERVICE_H_
#define BUSINESSSERVICE_H_

#include <hiredis.h>
#include <string>
#include <vector>
#include <mutex>
#include "RankVo.h"

#pragma once

using namespace std;

class Redis {
private:
	redisContext* conn = nullptr;
	redisReply* resp = nullptr;

	mutex lock;
	Redis();

	Redis(const Redis& rhs) = delete;
	Redis(const Redis&& rhs) = delete;
	void operator=(const Redis& rhs) = delete;
	void operator=(const Redis&& rhs) = delete;

public:
	static Redis* GetInstance() {
		static Redis instance;
		return &instance;
	}

	virtual ~Redis();

	void Set(const string& key, const string& value);

	string Get(const string& key);

	void Zadd(const string& key, const string& value, int num);

	void Zincr(const string& key, const string& value);

	vector<RankVo> Zrevrange(const string& key, int num);
};

#endif