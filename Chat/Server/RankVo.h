
#ifndef RANKVO_H_
#define RANKVO_H_
#include <string>
#include "RankVo.h"

#pragma once

using namespace std;

#include <string>

#pragma once


class RankVo {
private:
	char nickName[20];
	int point;
public:
	RankVo();

	RankVo(const RankVo& vo);

	RankVo& operator=(const RankVo& vo);

	virtual ~RankVo();

	void setNickName(const char* nickName) {
		strncpy(this->nickName, nickName, 20);
	}

	const char* getNickName() const {
		return nickName;
	}

	void setPoint(int point) {
		this->point = point;
	}

	const int getPoint() const {
		return point;
	}
};
#endif