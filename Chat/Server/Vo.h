/*
* Vo.h
*
*  Created on: 2019. 1. 28.
*      Author: choiis1207
*/

#ifndef VO_H_
#define VO_H_

#include <string.h>

class Vo {
private:
	char userId[20];
	char nickName[20];
	char roomName[20];
	char msg[512];
	int status;
	int direction;
	char password[10];
	char relationto[20];
	int relationcode;
public:
	Vo();

	Vo(const char* userId, const char* nickName, const char* roomName, const char* msg, int status, int direction, const char* password, const char* relationto, int relationcode);

	Vo(const Vo& vo);

	Vo& operator=(const Vo& vo);

	virtual ~Vo();

	int getDirection() const {
		return direction;
	}

	void setDirection(const int direction) {
		this->direction = direction;
	}

	void setUserId(const char* userId) {
		strncpy(this->userId, userId, 10);
	}

	void setRoomName(const char* roomName) {
		strncpy(this->roomName, roomName, 10);
	}

	void setPassword(const char* password) {
		strncpy(this->password, password, 10);
	}

	void setNickName(const char* nickName) {
		strncpy(this->nickName, nickName, 20);
	}

	void setMsg(const char* msg) {
		strncpy(this->msg, msg, 512);
	}

	void setRelationto(const char* relationto) {
		strncpy(this->relationto, relationto, 512);
	}

	void setRelationcode(const int relationcode) {
		this->relationcode = relationcode;
	}

	const char* getNickName() const {
		return nickName;
	}

	const char* getRoomName() const {
		return roomName;
	}

	int getStatus() const {
		return status;
	}

	void setStatus(const int status) {
		this->status = status;
	}

	const char* getUserId() const {
		return userId;
	}

	const char* getMsg() const {
		return msg;
	}

	const char* getPassword() const {
		return password;
	}

	const char* getRelationto() const {
		return relationto;
	}

	int getRelationcode() const {
		return relationcode;
	}
};

#endif /* VO_H_ */