/*
 * BusinessService.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#ifndef BUSINESSSERVICE_H_
#define BUSINESSSERVICE_H_

#include <winsock2.h>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <string>
#include "IocpService.h"
#include "MPool.h"
#include "common.h"
#include "CharPool.h"
#include "Dao.h"

// #pragma comment(lib, "Dao.h") 

// 서버에 접속한 유저 정보
// client 소켓에 대응하는 세션정보
typedef struct { // socket info
	char userName[NAME_SIZE];
	char roomName[NAME_SIZE];
	char userId[NAME_SIZE];
	int status;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	list<SOCKET> userList;
	CRITICAL_SECTION listCs;
} ROOM_DATA, *P_ROOM_DATA;

namespace Service {

class BusinessService {
private:
	// DB connection Object
	Dao* dao;
	// 중복로그인 방지에 쓰일 구조체
	unordered_set<string> idSet;
	// 서버에 접속한 유저 자료 저장
	unordered_map<SOCKET, PER_HANDLE_DATA> userMap;
	// 서버의 방 정보 저장
	unordered_map<string, ROOM_DATA> roomMap;

	// 임계영역에 필요한 객체
	// 커널모드 아니라 유저모드수준 동기화 사용할 예
	// 한 프로세스내의 동기화 이므로 크리티컬섹션 사용

	// idMap 동기화
	CRITICAL_SECTION idCs;
	// userMap 동기화
	CRITICAL_SECTION userCs;
	// roomMap 동기화
	CRITICAL_SECTION roomCs;

	IocpService::IocpService *iocpService;
public:
	// 생성자
	BusinessService();
	// 소멸자
	virtual ~BusinessService();
	// 초기 로그인
	// 세션정보 추가
	void InitUser(const char *id, SOCKET sock ,const char *nickName);
	// 접속 강제종료 로직
	void ClientExit(SOCKET sock);
	// 로그인 이전 로직처리
	// 세션값 없을 때 로직
	void StatusLogout(SOCKET sock, int direction, const char *message);
	// 대기실에서의 로직 처리
	// 세션값 있음
	void StatusWait(SOCKET sock, int status, int direction, const char *message);
	// 채팅방에서의 로직 처리
	// 세션값 있음
	void StatusChat(SOCKET sock, int status, int direction, const char *message);
	// 클라이언트에게 받은 데이터 복사후 구조체 해제
	string DataCopy(LPPER_IO_DATA ioInfo, int *status, int *direction);
	// 패킷 데이터 읽기
	short PacketReading(LPPER_IO_DATA ioInfo, short bytesTrans);

	bool SessionCheck(SOCKET sock);

	int GetStatus(SOCKET sock);

	const unordered_set<string>& getIdSet() const {
		return idSet;
	}

	const unordered_map<string, ROOM_DATA>& getRoomMap() const {
		return roomMap;
	}

	const unordered_map<SOCKET, PER_HANDLE_DATA>& getUserMap() const {
		return userMap;
	}

	IocpService::IocpService* getIocpService();

	const CRITICAL_SECTION& getIdCs() const {
		return idCs;
	}

	const CRITICAL_SECTION& getRoomCs() const {
		return roomCs;
	}

	const CRITICAL_SECTION& getUserCs() const {
		return userCs;
	}
};

} /* namespace Service */

#endif /* BUSINESSSERVICE_H_ */
