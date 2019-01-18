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
#include <list>
#include "IocpService.h"
#include "MPool.h"
#include "common.h"
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
	char nickname[NAME_SIZE];
	string password;
	int logMode;
} USER_DATA, *P_USER_DATA;

namespace Service {

class BusinessService {
private:
	unordered_map<string, USER_DATA> idMap;
	// 서버에 접속한 유저 자료 저장
	unordered_map<SOCKET, LPPER_HANDLE_DATA> userMap;
	// 서버의 방 정보 저장
	unordered_map<string, list<SOCKET> > roomMap;

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
	void InitUser(const char *id, SOCKET sock);
	// 접속 강제종료 로직
	void ClientExit(SOCKET sock);
	// 로그인 이전 로직처리
	// 세션값 없을 때 로직
	void StatusLogout(SOCKET sock, int status, int direction, char *message);
	// 대기실에서의 로직 처리
	// 세션값 있음
	void StatusWait(SOCKET sock, int status, int direction, char *message);
	// 채팅방에서의 로직 처리
	// 세션값 있음
	void StatusChat(SOCKET sock, int status, int direction, char *message);
	// 클라이언트에게 받은 데이터 복사후 구조체 해제
	char* DataCopy(LPPER_IO_DATA ioInfo, int *status, int *direction);
	// 패킷 데이터 읽기
	void PacketReading(LPPER_IO_DATA ioInfo, DWORD bytesTrans);

	const unordered_map<string, USER_DATA>& getIdMap() const {
		return idMap;
	}

	const unordered_map<string, list<SOCKET> >& getRoomMap() const {
		return roomMap;
	}

	const unordered_map<SOCKET, LPPER_HANDLE_DATA>& getUserMap() const {
		return userMap;
	}

	IocpService::IocpService* getIocpService();
};

} /* namespace Service */

#endif /* BUSINESSSERVICE_H_ */
