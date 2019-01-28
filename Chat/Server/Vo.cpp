/*
 * Vo.cpp
 *
 *  Created on: 2019. 1. 28.
 *      Author: choiis1207
 */

#include "Vo.h"

Vo::Vo() {
	strncpy(this->userId, "", 20);
	strncpy(this->nickName, "", 20);
	strncpy(this->roomName, "", 10);
	strncpy(this->password, "", 10);
	strncpy(this->msg, "", 512);
	this->status = 0;
	this->direction = 0;
}

Vo::Vo(const char* userId, const char* nickName, const char* roomName, const char* msg, int status,
	int direction, const char* password) {
	strncpy(this->userId, userId, 10);
	strncpy(this->nickName, nickName, 20);
	strncpy(this->roomName, roomName, 10);
	strncpy(this->password, password, 10);
	strncpy(this->msg, msg, 512);
	this->status = status;
	this->direction = direction;
}

Vo::~Vo() {
	// TODO Auto-generated destructor stub
}

