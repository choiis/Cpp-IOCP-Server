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
}

Vo::~Vo() {
	// TODO Auto-generated destructor stub
}
// 복사생성자
Vo::Vo(const Vo& vo) {
	strncpy(this->userId, vo.userId, 20);
	strncpy(this->nickName, vo.nickName, 20);
}
// 대입연산자
Vo& Vo::operator=(const Vo& vo) {
	strncpy(this->userId, vo.userId, 20);
	strncpy(this->nickName, vo.nickName, 20);

	return *this;
}

UserVo::UserVo() : Vo() {
	strncpy(this->password, "", 10);
}
UserVo::~UserVo() {
	// TODO Auto-generated destructor stub
}
// 복사생성자
UserVo::UserVo(const UserVo& vo) : Vo(vo) {
	strncpy(this->password, vo.password, 10);
}
// 대입연산자
UserVo& UserVo::operator=(const UserVo& vo) {
	Vo::operator=(vo);
	strncpy(this->password, vo.password, 10);
	return *this;
}

LogVo::LogVo() : Vo() {
	strncpy(this->roomName, "", 20);
	strncpy(this->msg, "", 512);
	this->status = 0;
	this->direction = 0;
	this->bytes = 0;
	strncpy(this->filename, "", 100);
	strncpy(this->nickname, "", 20);
	this->fileDir = "";
}
LogVo::~LogVo() {
	// TODO Auto-generated destructor stub
}
// 복사생성자
LogVo::LogVo(const LogVo& vo) : Vo(vo) {
	strncpy(this->roomName, vo.roomName, 20);
	strncpy(this->msg, vo.msg, 512);
	this->status = vo.status;
	this->direction = vo.direction;
	this->bytes = vo.bytes;
	strncpy(this->filename, vo.filename, 100);
	strncpy(this->nickname, vo.nickname, 20);
	this->fileDir = vo.fileDir;
}
// 대입연산자
LogVo& LogVo::operator=(const LogVo& vo) {
	Vo::operator=(vo);
	strncpy(this->roomName, vo.roomName, 20);
	strncpy(this->msg, vo.msg, 512);
	this->status = vo.status;
	this->direction = vo.direction;
	this->bytes = vo.bytes;
	strncpy(this->filename, vo.filename, 100);
	strncpy(this->nickname, vo.nickname, 20);
	this->fileDir = vo.fileDir;
	return *this;
}


RelationVo::RelationVo() : Vo() {
	strncpy(this->relationto, "", 20);
	this->relationcode = 0;
}
RelationVo::~RelationVo() {
	// TODO Auto-generated destructor stub
}
// 복사생성자
RelationVo::RelationVo(const RelationVo& vo) : Vo(vo) {
	strncpy(this->relationto, vo.relationto, 20);
	this->relationcode = vo.relationcode;
}
// 대입연산자
RelationVo& RelationVo::operator=(const RelationVo& vo) {
	Vo::operator=(vo);
	strncpy(this->relationto, vo.relationto, 20);
	this->relationcode = vo.relationcode;
	return *this;
}