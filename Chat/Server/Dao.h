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
#include <sys/timeb.h>
#include <string>
#include <time.h>
#include "Vo.h"

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

	void InsertUser(Vo& vo);

	void InsertLogin(Vo& vo);

	void InsertDirection(Vo& vo);

	void InsertChatting(Vo& vo);
};
#endif /* DAO_H_ */
