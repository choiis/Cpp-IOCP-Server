
#include "RankVo.h"

RankVo::RankVo() {
	strncpy(this->nickName, "", 10);
	this->point = 0;
};

RankVo::RankVo(const RankVo& vo) {
	strncpy(this->nickName, vo.nickName, 10);
	this->point = vo.point;
};

RankVo& RankVo::operator=(const RankVo& vo) {
	strncpy(this->nickName, vo.nickName, 10);
	this->point = vo.point;
};

RankVo::~RankVo() {

};
