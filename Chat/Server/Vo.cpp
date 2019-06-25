/*
* Vo.cpp
*
*  Created on: 2019. 1. 28.
*      Author: choiis1207
*/

#include "Vo.h"

Vo::Vo() : status(0), direction(0), relationcode(0) {
	strncpy(this->userId, "", 20);
	strncpy(this->nickName, "", 20);
	strncpy(this->roomName, "", 10);
	strncpy(this->password, "", 10);
	strncpy(this->msg, "", 512);
	strncpy(this->relationto, "", 20);
}

Vo::Vo(const char* userId, const char* nickName, const char* roomName, const char* msg, int status,
	int direction, const char* password, const char* relationto, int relationcode) : status(status), direction(direction), relationcode(relationcode) {
	strncpy(this->userId, userId, 20);
	strncpy(this->nickName, nickName, 20);
	strncpy(this->roomName, roomName, 20);
	strncpy(this->password, password, 10);
	strncpy(this->msg, msg, 512);
	strncpy(this->relationto, relationto, 20);
}

Vo::~Vo() {
	// TODO Auto-generated destructor stub
}
// 복사생성자
Vo::Vo(const Vo& vo) : status(vo.status), direction(vo.direction), relationcode(vo.relationcode) {
	strncpy(this->userId, vo.getUserId(), 20);
	strncpy(this->nickName, vo.getNickName(), 20);
	strncpy(this->roomName, vo.getRoomName(), 20);
	strncpy(this->password, vo.getPassword(), 10);
	strncpy(this->msg, vo.getMsg(), 512);
	strncpy(this->relationto, vo.getRelationto(), 20);
}
// 대입연산자
Vo& Vo::operator=(const Vo& vo) {
	this->status = vo.status;
	this->direction = vo.direction;
	this->relationcode = vo.relationcode;
	strncpy(this->userId, vo.getUserId(), 20);
	strncpy(this->nickName, vo.getNickName(), 20);
	strncpy(this->roomName, vo.getRoomName(), 20);
	strncpy(this->password, vo.getPassword(), 10);
	strncpy(this->msg, vo.getMsg(), 512);
	strncpy(this->relationto, vo.getRelationto(), 20);

	return *this;
}