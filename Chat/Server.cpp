//============================================================================
// Name        : Server.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <map>
#include <list>
#include "common.h"

using namespace std;

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
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[sizeof(PACKET_DATA)]; // 이거때문에 PACKET_DATA 크기 고정 필요한듯? 추후 실험 필요
	int serverMode;
} PER_IO_DATA, *LPPER_IO_DATA;

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	char nickname[NAME_SIZE];
	string password;
	int logMode;
} USER_DATA, *P_USER_DATA;

// 계정과 비밀번호를 담을 자료구조
map<string, USER_DATA> idMap;
// 서버에 접속한 유저 자료 저장
map<SOCKET, LPPER_HANDLE_DATA> userMap;
// 서버의 방 정보 저장
map<string, list<SOCKET> > roomMap;

// 임계영역에 필요한 객체
// 커널모드 아니라 유저모드수준 동기화 사용할 예
// 한 프로세스내의 동기화 이므로 크리티컬섹션 사용
CRITICAL_SECTION cs;

// 한명에게 메세지 전달
void SendToOneMsg(LPPER_IO_DATA ioInfo, char *msg, SOCKET mySock, int status) {

	delete ioInfo;
	ioInfo = new PER_IO_DATA;

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	P_PACKET_DATA packet = new PACKET_DATA;

	strncpy(packet->message, msg, BUF_SIZE);
	packet->clientStatus = status;
	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
	ioInfo->serverMode = WRITE;

	WSASend(mySock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);

	delete packet;
}

// 같은 방의 사람들에게 메세지 전달
// 위아래로 lock 필요
void SendToRoomMsg(LPPER_IO_DATA ioInfo, char *msg, list<SOCKET> lists,
		int status) {

	delete ioInfo;
	ioInfo = new PER_IO_DATA;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	P_PACKET_DATA packet = new PACKET_DATA;

	strncpy(packet->message, msg, BUF_SIZE);
	packet->clientStatus = status;
	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
	ioInfo->serverMode = WRITE;

	list<SOCKET>::iterator iter;

	for (iter = lists.begin(); iter != lists.end(); iter++) {
		WSASend((*iter), &(ioInfo->wsaBuf), 1,
		NULL, 0, &(ioInfo->overlapped), NULL);
	}

	delete packet;
}

// Recv 공통함수
void Recv(SOCKET sock, int recvBytes, DWORD flags) {
	LPPER_IO_DATA ioInfo = new PER_IO_DATA;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ; // GetQueuedCompletionStatus 이후 분기가 send로 갈수 있게

	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, (LPDWORD) &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}

// 초기 로그인
// 세션정보 추가
void InitUser(P_PACKET_DATA packet, LPPER_IO_DATA ioInfo, SOCKET sock) {

	char msg[BUF_SIZE];
	char name[NAME_SIZE];
	DWORD flags = 0;
	int recvBytes = 0;

	// memcpy 부분 삭제

	strncpy(name, idMap.find(packet->message)->second.nickname, NAME_SIZE);

	LPPER_HANDLE_DATA handleInfo = new PER_HANDLE_DATA;
	cout << "START user : " << name << endl;
	cout << "sock : " << sock << endl;
	// 유저의 상태 정보 초기화
	// ioInfo = new PER_IO_DATA;
	handleInfo->status = STATUS_WAITING;

	strncpy(handleInfo->userId, packet->message, NAME_SIZE);
	strncpy(handleInfo->userName, name, NAME_SIZE);
	strncpy(handleInfo->roomName, "", NAME_SIZE);
	EnterCriticalSection(&cs);
	userMap.insert(pair<SOCKET, LPPER_HANDLE_DATA>(sock, handleInfo));

	// id의 key값은 유저 이름이 아니라 아이디!!
	idMap.find(packet->message)->second.logMode = NOW_LOGIN;

	cout << "현재 접속 인원 수 : " << userMap.size() << endl;

	LeaveCriticalSection(&cs);

	delete ioInfo;

	strncpy(msg, "입장을 환영합니다!\n", BUF_SIZE);
	strcat(msg, waitRoomMessage);
	SendToOneMsg(ioInfo, msg, sock, STATUS_WAITING);

	Recv(sock, recvBytes, flags);
}

// 접속 강제종료 로직
void ClientExit(SOCKET sock, LPPER_IO_DATA ioInfo) {
	// 이부분 고민 필요...
	cout << "강제 종료 " << endl;
	EnterCriticalSection(&cs);
	if (userMap.find(sock) != userMap.end()) {

		// 세션정보 있을때는 => 세션정보 방 정보 제거 필요
		char roomName[NAME_SIZE];
		char name[NAME_SIZE];
		char id[NAME_SIZE];

		strncpy(roomName, userMap.find(sock)->second->roomName, NAME_SIZE);
		strncpy(name, userMap.find(sock)->second->userName, NAME_SIZE);
		strncpy(id, userMap.find(sock)->second->userId, NAME_SIZE);
		// 로그인 상태 원상복구
		idMap.find(id)->second.logMode = NOW_LOGOUT;

		// 방이름 임시 저장
		if (strcmp(roomName, "") != 0) { // 방에 접속중인 경우
			// 나가는 사람 정보 out

			if (roomMap.find(roomName) != roomMap.end()) { // null 체크 우선
				(roomMap.find(roomName)->second).remove(sock);

				if ((roomMap.find(roomName)->second).size() == 0) { // 방인원 0명이면 방 삭제

					roomMap.erase(roomName);

				} else {
					char sendMsg[BUF_SIZE];
					// 이름먼저 복사 NAME_SIZE까지
					strncpy(sendMsg, name, NAME_SIZE);
					strcat(sendMsg, " 님이 나갔습니다!");

					if (userMap.find(sock) != userMap.end()) { // null 검사
						if (roomMap.find(userMap.find(sock)->second->roomName)
								!= roomMap.end()) { // null 검사
							SendToRoomMsg(ioInfo, sendMsg,
									roomMap.find(
											userMap.find(sock)->second->roomName)->second,
									STATUS_CHATTIG);
						}
					}

				}

			}

		}
		userMap.erase(sock); // 접속 소켓 정보 삭제
		cout << "현재 접속 인원 수 : " << userMap.size() << endl;
	}
	LeaveCriticalSection(&cs);
	closesocket(sock);
	delete ioInfo;
}

// 로그인 이전 로직처리
// 세션값 없을 때 로직
void StatusLogout(SOCKET sock, P_PACKET_DATA packet, LPPER_IO_DATA ioInfo) {

	int recvBytes = 0;
	DWORD flags = 0;

	if (packet->clientStatus == STATUS_LOGOUT) {

		// memcpy 부분 삭제
		char msg[BUF_SIZE];

		if (packet->direction == USER_MAKE) { // 1번 계정생성
			cout << "USER MAKE" << endl;
			// 유효성 검증 필요
			EnterCriticalSection(&cs);
			if (idMap.find(packet->message) == idMap.end()) { // ID 중복체크

				USER_DATA userData;
				userData.password = string(packet->message2);
				strncpy(userData.nickname, packet->message3, NAME_SIZE);
				userData.logMode = NOW_LOGOUT;

				idMap.insert(
						pair<string, USER_DATA>(string(packet->message),
								userData));

				strncpy(msg, packet->message, BUF_SIZE);
				strcat(msg, " 계정 생성 완료!\n");
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(ioInfo, msg, sock, STATUS_LOGOUT);
			} else { // ID중복있음
				strncpy(msg, "중복된 아이디가 있습니다!\n", BUF_SIZE);
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(ioInfo, msg, sock, STATUS_LOGOUT);
			}
			LeaveCriticalSection(&cs);
		} else if (packet->direction == USER_ENTER) { // 2번 로그인 시도
			cout << "USER ENTER" << endl;
			if (idMap.find(packet->message) == idMap.end()) { // 계정 없음
				strncpy(msg, "없는 계정입니다 아이디를 확인하세요!\n", BUF_SIZE);
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(ioInfo, msg, sock, STATUS_LOGOUT);
			} else if (idMap.find(packet->message)->second.password.compare(
					packet->message2) == 0) { // 비밀번호 일치

				if (idMap.find(packet->message)->second.logMode == NOW_LOGIN) { // 중복로그인 유효성 검사
					strncpy(msg, "중복 로그인은 안됩니다!\n", BUF_SIZE);
					strcat(msg, loginBeforeMessage);
					SendToOneMsg(ioInfo, msg, sock, STATUS_LOGOUT);
				} else { // 중복로그인 X
					InitUser(packet, ioInfo, sock); // 세션정보 저장
				}

			} else { // 비밀번호 틀림
				strncpy(msg, "비밀번호 틀림!\n", BUF_SIZE);
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(ioInfo, msg, sock, STATUS_LOGOUT);
			}

		} else if (packet->direction == USER_LIST) { // 유저들 정보 요청
			cout << "USER LIST" << endl;
			char userList[BUF_SIZE];
			strncpy(userList, "아이디 정보 리스트", BUF_SIZE);
			if (idMap.size() == 0) {
				strcat(userList, " 없음");
			} else {
				map<string, USER_DATA>::iterator iter;
				for (iter = idMap.begin(); iter != idMap.end(); ++iter) {
					strcat(userList, "\n");
					strcat(userList, (iter->first).c_str());
				}
			}

			SendToOneMsg(ioInfo, userList, sock, STATUS_LOGOUT);
		}
	}
	// 세션값 없음 => 로그인 이전 분기 끝

	Recv(sock, recvBytes, flags);
}

// 대기실에서의 로직 처리
// 세션값 있음
void StatusWait(SOCKET sock, P_PACKET_DATA packet, LPPER_IO_DATA ioInfo) {

	char name[BUF_SIZE];
	char msg[BUF_SIZE];
	int direction = packet->direction;
	// 세션에서 이름 정보 리턴
	strncpy(name, userMap.find(sock)->second->userName, BUF_SIZE);
	strncpy(msg, packet->message, BUF_SIZE);
	if (direction == ROOM_MAKE) { // 새로운 방 만들때
		cout << "ROOM MAKE" << endl;
		// 유효성 검증 먼저
		if (roomMap.count(msg) != 0) { // 방이름 중복
			strncpy(msg, "이미 있는 방 이름입니다!\n", BUF_SIZE);
			strcat(msg, waitRoomMessage);
			SendToOneMsg(ioInfo, msg, sock, STATUS_WAITING);
			// 중복 케이스는 방 만들 수 없음
		} else { // 방이름 중복 아닐 때만 개설

			EnterCriticalSection(&cs);
			// 새로운 방 정보 저장
			list<SOCKET> chatList;
			chatList.push_back(sock);
			roomMap.insert(pair<string, list<SOCKET> >(msg, chatList));

			// User의 상태 정보 바꾼다
			if (userMap.find(sock) != userMap.end()) { // 유효성 체크 우선
				strncpy((userMap.find(sock))->second->roomName, msg, NAME_SIZE);
				(userMap.find(sock))->second->status =
				STATUS_CHATTIG;

			}

			LeaveCriticalSection(&cs);
			strcat(msg, " 방이 개설되었습니다.");
			strcat(msg, chatRoomMessage);
			cout << "현재 서버의 방 갯수 : " << roomMap.size() << endl;
			SendToOneMsg(ioInfo, msg, sock, STATUS_CHATTIG);
		}

	} else if (direction == ROOM_ENTER) { // 방 입장 요청
		cout << "ROOM ENTER" << endl;

		// 유효성 검증 먼저
		if (roomMap.count(msg) == 0) { // 방 못찾음
			strncpy(msg, "없는 방 입니다!\n", sizeof("없는 방 입니다!\n"));
			strcat(msg, waitRoomMessage);
			SendToOneMsg(ioInfo, msg, sock, STATUS_WAITING);
		} else {
			EnterCriticalSection(&cs);

			if (roomMap.find(msg) != roomMap.end()) {
				roomMap.find(msg)->second.push_back(sock);
			}
			if (userMap.find(sock) != userMap.end()) {
				strncpy(userMap.find(sock)->second->roomName, msg, NAME_SIZE);
				userMap.find(sock)->second->status = STATUS_CHATTIG;
			}

			LeaveCriticalSection(&cs);

			char sendMsg[BUF_SIZE];
			strncpy(sendMsg, name, BUF_SIZE);
			strcat(sendMsg, " 님이 입장하셨습니다. ");
			strcat(sendMsg, chatRoomMessage);

			if (roomMap.find(msg) != roomMap.end()) { // 안전성 검사
				EnterCriticalSection(&cs);
				SendToRoomMsg(ioInfo, sendMsg, roomMap.find(msg)->second,
				STATUS_CHATTIG);
				LeaveCriticalSection(&cs);
			}
		}

	} else if (strcmp(msg, "1") == 0) { // 방 정보 요청시

		map<string, list<SOCKET> >::iterator iter;
		char str[BUF_SIZE];
		if (roomMap.size() == 0) {
			strncpy(str, "만들어진 방이 없습니다", BUF_SIZE);
		} else {
			strncpy(str, "방 정보 리스트", BUF_SIZE);
			EnterCriticalSection(&cs);
			// 방정보를 문자열로 만든다
			for (iter = roomMap.begin(); iter != roomMap.end(); iter++) {
				char num[4];
				strcat(str, "\n");
				strcat(str, iter->first.c_str());
				strcat(str, " : ");
				sprintf(num, "%d", (iter->second).size());
				strcat(str, num);
				strcat(str, "명");
			}
			LeaveCriticalSection(&cs);
		}

		SendToOneMsg(ioInfo, str, sock, STATUS_WAITING);
	} else if (strcmp(msg, "4") == 0) { // 유저 정보 요청시
		char str[BUF_SIZE];

		map<SOCKET, LPPER_HANDLE_DATA>::iterator iter;
		strncpy(str, "유저 정보 리스트", BUF_SIZE);
		// 유저 정보를 문자열로 만든다
		EnterCriticalSection(&cs);
		for (iter = userMap.begin(); iter != userMap.end(); iter++) {
			strcat(str, "\n");
			strcat(str, (iter->second)->userName);
			strcat(str, " : ");
			if ((iter->second)->status == STATUS_WAITING) {
				strcat(str, "대기실 ");
			} else {
				strcat(str, (iter->second)->roomName);
			}
		}
		LeaveCriticalSection(&cs);
		SendToOneMsg(ioInfo, str, sock, STATUS_WAITING);
	} else if (direction == WHISPER) { // 귓속말
		cout << "WHISPER " << endl;
		strncpy(name, packet->message2, NAME_SIZE);

		map<SOCKET, LPPER_HANDLE_DATA>::iterator iter;
		char sendMsg[BUF_SIZE];
		EnterCriticalSection(&cs);
		if (strcmp(name, userMap.find(sock)->second->userName) == 0) { // 본인에게 쪽지
			strncpy(sendMsg, "자신에게 쪽지를 보낼수 없습니다\n", BUF_SIZE);
			strcat(sendMsg, waitRoomMessage);
			SendToOneMsg(ioInfo, sendMsg, sock, STATUS_WAITING);
		} else {
			bool find = false;
			for (iter = userMap.begin(); iter != userMap.end(); iter++) {
				if (strcmp(iter->second->userName, name) == 0) {
					find = true;

					strncpy(sendMsg, userMap.find(sock)->second->userName,
					BUF_SIZE);
					strcat(sendMsg, " 님에게 온 귓속말 : ");
					strcat(sendMsg, msg);
					SendToOneMsg(ioInfo, sendMsg, iter->first, STATUS_WHISPER);
					break;
				}
			}
			// 귓속말 대상자 못찾음
			if (!find) {
				strncpy(sendMsg, name, BUF_SIZE);
				strcat(sendMsg, " 님을 찾을 수 없습니다");
				SendToOneMsg(ioInfo, sendMsg, sock, STATUS_WAITING);
			}
		}

		LeaveCriticalSection(&cs);

	} else if (strcmp(msg, "6") == 0) { // 로그아웃
		EnterCriticalSection(&cs);

		string userId = idMap.find(userMap.find(sock)->second->userId)->first;
		userMap.erase(sock); // 접속 소켓 정보 삭제
		idMap.find(userId)->second.logMode = NOW_LOGOUT;
		LeaveCriticalSection(&cs);
		char sendMsg[BUF_SIZE];

		strncpy(sendMsg, loginBeforeMessage, BUF_SIZE);
		SendToOneMsg(ioInfo, sendMsg, sock, STATUS_LOGOUT);
	} else { // 그외 명령어 입력
		char sendMsg[BUF_SIZE];

		strncpy(sendMsg, errorMessage, BUF_SIZE);
		strcat(sendMsg, waitRoomMessage);
		// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다
		SendToOneMsg(ioInfo, sendMsg, sock, STATUS_WAITING);
	}
}

// 채팅방에서의 로직 처리
// 세션값 있음
void StatusChat(SOCKET sock, P_PACKET_DATA packet, LPPER_IO_DATA ioInfo) {

	char name[NAME_SIZE];
	char msg[BUF_SIZE];

	// 세션에서 이름 정보 리턴
	strncpy(name, userMap.find(sock)->second->userName, NAME_SIZE);
	strncpy(msg, packet->message, BUF_SIZE);

	if (strcmp(msg, "out") == 0) { // 채팅방 나감

		char sendMsg[BUF_SIZE];
		strncpy(sendMsg, name, BUF_SIZE);
		strcat(sendMsg, " 님이 나갔습니다!");

		if (userMap.find(sock) != userMap.end()) { // null 검사
			if (roomMap.find(userMap.find(sock)->second->roomName)
					!= roomMap.end()) { // null 검사
				EnterCriticalSection(&cs);
				SendToRoomMsg(ioInfo, sendMsg,
						roomMap.find(userMap.find(sock)->second->roomName)->second,
						STATUS_CHATTIG);
				LeaveCriticalSection(&cs);
			}
		}

		char roomName[NAME_SIZE];
		EnterCriticalSection(&cs);
		if (userMap.find(sock) != userMap.end()) {
			strncpy(roomName, userMap.find(sock)->second->roomName, NAME_SIZE);
			// 방이름 임시 저장
			strncpy(userMap.find(sock)->second->roomName, "", NAME_SIZE);
			userMap.find(sock)->second->status = STATUS_WAITING;
		}

		strncpy(msg, waitRoomMessage, BUF_SIZE);
		SendToOneMsg(ioInfo, msg, sock, STATUS_WAITING);
		cout << "나가는 사람 name : " << name << endl;
		cout << "나가는 방 name : " << roomName << endl;

		// 나가는 사람 정보 out
		(roomMap.find(roomName)->second).remove(sock);

		if ((roomMap.find(roomName)->second).size() == 0) { // 방인원 0명이면 방 삭제
			roomMap.erase(roomName);
		}
		LeaveCriticalSection(&cs);
	} else { // 채팅방에서 채팅중

		char sendMsg[BUF_SIZE];
		strncpy(sendMsg, name, BUF_SIZE);
		strcat(sendMsg, " : ");
		strcat(sendMsg, msg);

		if (userMap.find(sock) != userMap.end()) { // null 검사
			if (roomMap.find(userMap.find(sock)->second->roomName)
					!= roomMap.end()) { // null 검사
				EnterCriticalSection(&cs);
				SendToRoomMsg(ioInfo, sendMsg,
						roomMap.find(userMap.find(sock)->second->roomName)->second,
						STATUS_CHATTIG);
				LeaveCriticalSection(&cs);
			}
		}
	}

}

// Server 컴퓨터  CPU 개수만큼 스레드 생성될것
// 내부 로직은 각 함수별로 처리
unsigned WINAPI HandleThread(LPVOID pCompPort) {

	// Completion port객체
	HANDLE hComPort = (HANDLE) pCompPort;
	SOCKET sock;
	DWORD bytesTrans;
	// LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;
	P_PACKET_DATA packet;

	while (true) {

		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &sock,
				(LPOVERLAPPED*) &ioInfo, INFINITE);

		if (READ == ioInfo->serverMode) { // Recv 끝난경우
			cout << "message received" << endl;

			// IO 완료후 동작 부분
			int recvBytes = 0;
			DWORD flags = 0;
			// 패킷을 받고 이름과 메세지를 초기 저장
			packet = new PACKET_DATA;
			memcpy(packet, ioInfo->buffer, sizeof(PACKET_DATA));

			if (bytesTrans == 0) { // 접속 끊김 콘솔 강제 종료
				// 콘솔 강제종료 처리
				ClientExit(sock, ioInfo);

			} else if (packet->clientStatus == 0 && packet->direction == 0) { // 이상한 패킷 필터링
				cout << "Filter" << endl;
			} else if (userMap.find(sock) == userMap.end()) { // 세션값 없음 => 로그인 이전 분기
					// 로그인 이전 로직 처리
				StatusLogout(sock, packet, ioInfo);
			} else { // 세션값 있을때 => 대기방 또는 채팅방 상태

				int status = userMap.find(sock)->second->status;

				if (status == STATUS_WAITING) { // 대기실 케이스
					// 대기실 처리 함수
					StatusWait(sock, packet, ioInfo);
				} else if (status == STATUS_CHATTIG) { // 채팅 중 케이스
					// 채팅방 처리 함수
					StatusChat(sock, packet, ioInfo);
				}

				// Recv는 계속한다
				Recv(sock, recvBytes, flags);
			}
		} else { // Send 끝난경우
			cout << "message send" << endl;
			// delete ioInfo;
		}
	}
	return 0;
}

int main(int argc, char* argv[]) {

	WSADATA wsaData;
	HANDLE hComPort;
	SYSTEM_INFO sysInfo;
	LPPER_IO_DATA ioInfo = NULL;
	LPPER_HANDLE_DATA handleInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAdr;
	int recvBytes = 0;
	DWORD flags = 0;
	if (argc != 2) {
		cout << "Usage : " << argv[0] << endl;
		exit(1);
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup() error!" << endl;
		exit(1);
	}

	// Completion Port 생성
	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);

	int process = sysInfo.dwNumberOfProcessors;
	cout << "Server CPU num : " << process << endl;

	// CPU 갯수 만큼 스레드 생성
	for (int i = 0; i < process; i++) {
		// 만들어진 HandleThread를 hComPort CP 오브젝트에 할당한다
		_beginthreadex(NULL, 0, HandleThread, (LPVOID) hComPort, 0, NULL);
		// 첫번째는 security관련 NULL 쓰며됨
		// 다섯번째 0은 스레드 곧바로 시작 의미
		// 여섯번째는 스레드 아이디
	}

	// Overlapped IO가능 소켓을 만든다
	// TCP 통신할것
	hServSock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
	WSA_FLAG_OVERLAPPED);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = PF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY뜻은 어느 IP에서 접속이 와도 요청 수락한다는 뜻
	servAdr.sin_port = htons(atoi(argv[1]));

	if (bind(hServSock, (SOCKADDR*) &servAdr, sizeof(servAdr)) == SOCKET_ERROR) {
		cout << "bind() error!" << endl;
		exit(1);
	}

	if (listen(hServSock, 5) == SOCKET_ERROR) {
		// listen의 두번째 인자는 접속 대기 큐의 크기
		// accept작업 다 끝나기전에 대기 할 공간
		cout << "listen() error!" << endl;
		exit(1);
	}

	// 임계영역 Object 생성
	InitializeCriticalSection(&cs);

	cout << "Server ready listen" << endl;
	cout << "port number : " << argv[1] << endl;

	while (true) {
		SOCKET hClientSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);
		cout << "accept wait" << endl;
		hClientSock = accept(hServSock, (SOCKADDR*) &clntAdr, &addrLen);

		handleInfo = new PER_HANDLE_DATA;

		cout << "Connected client IP " << inet_ntoa(clntAdr.sin_addr) << endl;

		// Completion Port 와 accept한 소켓 연결
		CreateIoCompletionPort((HANDLE) hClientSock, hComPort,
				(DWORD) hClientSock, 0);

		Recv(hClientSock, recvBytes, flags);

		// 초기 접속 메세지 Send
		char msg[BUF_SIZE];
		strncpy(msg, "접속을 환영합니다!\n", BUF_SIZE);
		strcat(msg, loginBeforeMessage);
		SendToOneMsg(ioInfo, msg, hClientSock, STATUS_LOGOUT);

		delete handleInfo;
	}

	// 임계영역 Object 반환
	DeleteCriticalSection(&cs);
	return 0;
}
