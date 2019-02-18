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
	CRITICAL_SECTION cs;

public:
	Dao();
	virtual ~Dao();

	Vo& selectUser(Vo& vo);

	void UpdateUser(Vo& vo);

	void LogoutUser(Vo& vo);

	void InsertUser(Vo& vo);

	void InsertLogin(Vo& vo);

	void InsertDirection(Vo& vo);

	void InsertChatting(Vo& vo);

	int InsertRelation(Vo& vo);

	Vo& findUserId(Vo& vo);

	vector<Vo> selectFriends(Vo& vo);

	Vo& selectOneFriend(Vo& vo);

	int DeleteRelation(Vo& vo);
};
#endif /* DAO_H_ */
