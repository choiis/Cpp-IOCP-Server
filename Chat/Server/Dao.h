/*
* IocpService.h
*
*  Created on: 2019. 1. 28.
*      Author: choiis1207
*/

#ifndef DAO_H_
#define DAO_H_

#include <iostream>
#include <windows.h>
#include <sqltypes.h>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>
#include <mutex>
#include "Vo.h"

using namespace std;

using std::string;

class Dao {
private:
	SQLHENV hEnv;
	SQLHDBC hDbc;
	SQLHSTMT hStmt;
	SQLRETURN res;
	// idMap µø±‚»≠
	mutex lock;

public:
	Dao();
	virtual ~Dao();

	UserVo selectUser(UserVo& vo);

	void UpdateUser(const LogVo& vo);

	void InsertUser(const UserVo& vo);

	void InsertLogin(const LogVo& vo);

	void InsertDirection(const LogVo& vo);

	void InsertChatting(const LogVo& vo);

	int InsertRelation(const RelationVo& vo);

	RelationVo findUserId(const RelationVo& vo);

	vector<RelationVo> selectFriends(const RelationVo& vo);

	RelationVo selectOneFriend(const RelationVo& vo);

	int DeleteRelation(const RelationVo& vo);

	int InsertFiles(const LogVo& vo);
};
#endif /* DAO_H_ */
