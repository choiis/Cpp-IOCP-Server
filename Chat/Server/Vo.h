/*
* Vo.h
*
*  Created on: 2019. 1. 28.
*      Author: choiis1207
*/

#ifndef VO_H_
#define VO_H_

#include <string>
using namespace std;

class Vo {
private:
	char userId[20];
	char nickName[20];
protected:
	Vo();

	Vo(const Vo& vo);

	Vo& operator=(const Vo& vo);
public:

	virtual ~Vo();

	void setUserId(const char* userId) {
		strncpy(this->userId, userId, 10);
	}

	void setNickName(const char* nickName) {
		strncpy(this->nickName, nickName, 20);
	}


	const char* getNickName() const {
		return nickName;
	}

	const char* getUserId() const {
		return userId;
	}

};

class UserVo :public Vo {
private:
	char password[10];
public:
	UserVo();

	UserVo(const UserVo& vo);

	UserVo& operator=(const UserVo& vo);

	virtual ~UserVo();

	void setPassword(const char* password) {
		strncpy(this->password, password, 10);
	}

	const char* getPassword() const {
		return password;
	}
};

class LogVo : public Vo {
private:
	char roomName[20];
	char msg[512];
	int status;
	int direction;
	char filename[100];
	char nickname[20];
	long bytes;
	string fileDir;
public:
	LogVo();

	LogVo(const LogVo& vo);

	LogVo& operator=(const LogVo& vo);

	virtual ~LogVo();

	void setRoomName(const char* roomName) {
		strncpy(this->roomName, roomName, 20);
	}

	const char* getRoomName() const {
		return roomName;
	}

	void setMsg(const char* msg) {
		strncpy(this->msg, msg, 512);
	}

	const char* getMsg() const {
		return msg;
	}

	void setStatus(int status) {
		this->status = status;
	}

	int getStatus() const {
		return status;
	}

	void setDirection(int direction) {
		this->direction = direction;
	}

	int getDirection() const{
		return direction;
	}

	void setFilename(const char* filename) {
		strncpy(this->filename, filename, 20);
	}

	const char* getFilename() const {
		return filename;
	}

	void setNickname(const char* nickname) {
		strncpy(this->nickname, nickname, 20);
	}

	const char* getNickname() const {
		return nickname;
	}

	void setBytes(long bytes) {
		this->bytes = bytes;
	}

	long getBytes() const {
		return bytes;
	}

	void setFileDir(const string& fileDir) {
		this->fileDir = fileDir;
	}

	string getFileDir() const {
		return fileDir;
	}
};

class RelationVo : public Vo {
private:
	char relationto[20];
	int relationcode;
public:
	RelationVo();

	RelationVo(const RelationVo& vo);

	RelationVo& operator=(const RelationVo& vo);

	virtual ~RelationVo();

	void setRelationto(const char* relationto) {
		strncpy(this->relationto, relationto, 20);
	}

	const char* getRelationto() const {
		return relationto;
	}

	void setRelationcode(int relationcode) {
		this->relationcode = relationcode;
	}

	int getRelationcode() const {
		return relationcode;
	}

};
#endif /* VO_H_ */
