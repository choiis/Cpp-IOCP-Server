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

	void UpdateUser(const Vo& vo);

	void InsertUser(const Vo& vo);

	void InsertLogin(const Vo& vo);

	void InsertDirection(const Vo& vo);

	void InsertChatting(const Vo& vo);

	int InsertRelation(const Vo& vo);

	Vo& findUserId(const Vo& vo);

	vector<Vo> selectFriends(const Vo& vo);

	Vo& selectOneFriend(const Vo& vo);

	int DeleteRelation(const Vo& vo);
};
#endif /* DAO_H_ */
