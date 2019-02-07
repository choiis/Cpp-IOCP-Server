/*
* IocpService.h
*
*  Created on: 2019. 1. 28.
*      Author: choiis1207
*/

#include "Dao.h"


Dao::Dao() {
	InitializeCriticalSectionAndSpinCount(&cs, 2000);
	// 환경 핸들러 할당
	SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
	// ODBC 드라이버 버전 명시
	SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER);
	// 연결 핸들러 할당
	SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);

	// DB connection
	res = SQLDriverConnect(hDbc, NULL,
		(SQLCHAR*)"DRIVER={SQL Server};SERVER=10.10.55.62, 1433; DATABASE=server_DB; UID=23460; PWD=qw1324..;",
		SQL_NTS, NULL, 1024, NULL, SQL_DRIVER_NOPROMPT); //접속 조건 입력. (성공 = 1, 실패 = -1 리턴)
	if (res == 1) {
		std::cout << "DB connection 성공" << std::endl;
	}
	
}

Dao::~Dao() {
	DeleteCriticalSection(&cs);
	if (hStmt) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	}
	if (hDbc){
		SQLDisconnect(hDbc);
	}
	if (hDbc){
		SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
	}
	if (hDbc){
		SQLFreeHandle(SQL_HANDLE_ENV, hDbc);
	}
}
	
// ID정보 select
Vo& Dao::selectUser(Vo& vo){
	EnterCriticalSection(&this->cs);
	res = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	char query[512] = "select userid,password,nickname from cso_id where userid = ? ";

	SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0, (SQLCHAR*)vo.getUserId(), sizeof(vo.getUserId()), NULL);
	SQLPrepare(hStmt, (SQLCHAR*)query, SQL_NTS);
	
	char userid[10];
	char password[10];
	char nickname[20];
	SQLINTEGER num1, num2, num3;

	SQLBindCol(hStmt, 1, SQL_C_CHAR, userid, sizeof(userid), &num1);
	SQLBindCol(hStmt, 2, SQL_C_CHAR, password, sizeof(password), &num2);
	SQLBindCol(hStmt, 3, SQL_C_CHAR, nickname, sizeof(nickname), &num3);
	
	res = SQLExecute(hStmt);

	if (SQLFetch(hStmt) != SQL_NO_DATA) {
		vo.setPassword(password);
		vo.setNickName(nickname);
	}
	else {
		vo.setUserId("");
	}

	if (hStmt != SQL_NULL_HSTMT)
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

	LeaveCriticalSection(&this->cs);
	return vo;
}


// ID정보 update
void Dao::UpdateUser(Vo& vo){
	EnterCriticalSection(&this->cs);
	
	res = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	

	char query[512] = "update cso_id set lastlogdate = getdate() where userid = ? ";

	res = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0, (SQLCHAR*)vo.getUserId(), sizeof(vo.getUserId()), NULL);

	SQLPrepare(hStmt, (SQLCHAR*)query, SQL_NTS);

	res = SQLExecute(hStmt);

	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	LeaveCriticalSection(&this->cs);
}

// ID정보 insert
void Dao::InsertUser(Vo& vo) {
	EnterCriticalSection(&this->cs);
	res = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	
	char query[512] = "insert into cso_id(userid, password , nickname , regdate) values(? ,? ,? ,getdate() ) ";
	
	res = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0, (SQLCHAR*)vo.getUserId(), sizeof(vo.getUserId()), NULL);
	res = SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0, (SQLCHAR*)vo.getPassword(), sizeof(vo.getPassword()), NULL);
	res = SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, (SQLCHAR*)vo.getNickName(), sizeof(vo.getNickName()), NULL);
	
	SQLPrepare(hStmt, (SQLCHAR*) query, SQL_NTS);
	
	res = SQLExecute(hStmt);

	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	LeaveCriticalSection(&this->cs);
}

// 로그인 정보 insert
void Dao::InsertLogin(Vo& vo) {
	EnterCriticalSection(&this->cs);
	res = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	

	char query[512] = "insert into cso_login(userid, logindate , nickname) values(? ,getdate() ,? ) ";
	
	SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0, (SQLCHAR*) vo.getUserId(), sizeof(vo.getUserId()), NULL);
	SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, (SQLCHAR*) vo.getNickName(), sizeof(vo.getNickName()), NULL);

	SQLPrepare(hStmt, (SQLCHAR*) query, SQL_NTS);

	res = SQLExecute(hStmt);

	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	LeaveCriticalSection(&this->cs);
}

// 지시 로그 insert
void Dao::InsertDirection(Vo& vo) {
	EnterCriticalSection(&this->cs);

	res = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	
	char query[512] = "insert into cso_direction(nickname, logdate, status, direction , message) values(? ,getdate() ,? ,? ,? ) ";

	SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, (SQLCHAR*)vo.getNickName(), sizeof(vo.getNickName()), NULL);

	int status = vo.getStatus();
	int direction = vo.getDirection();

	SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &status, 0, NULL);
	SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &direction, 0, NULL);
	SQLBindParameter(hStmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 512, 0, (SQLCHAR*)vo.getMsg(), sizeof(vo.getMsg()), NULL);

	SQLPrepare(hStmt, (SQLCHAR*)query, SQL_NTS);

	res = SQLExecute(hStmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	LeaveCriticalSection(&this->cs);
}

// 채팅 로그 insert
void  Dao::InsertChatting(Vo& vo) {
	EnterCriticalSection(&this->cs);
	res = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
	
	char query[512] = "insert into cso_chatting(nickname,logdate,roomname,message)  values(? ,getdate() , ? , ?)";
	
	SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, (SQLCHAR*)vo.getNickName(), sizeof(vo.getNickName()), NULL);
	SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0, (SQLCHAR*)vo.getRoomName(), sizeof(vo.getRoomName()), NULL);
	SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 512, 0, (SQLCHAR*)vo.getMsg(), sizeof(vo.getMsg()), NULL);

	SQLPrepare(hStmt, (SQLCHAR*)query, SQL_NTS);

	res = SQLExecute(hStmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	LeaveCriticalSection(&this->cs);
}