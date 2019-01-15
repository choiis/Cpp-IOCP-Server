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
void SendToOneMsg(char *msg, SOCKET mySock, int status) {

	LPPER_IO_DATA ioInfo = new PER_IO_DATA;

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	P_PACKET_DATA packet = new PACKET_DATA;

	int len = strlen(msg) + 1;
	char *p;
	p = new char[len + (3 * sizeof(int))];
	memcpy(p, &len, 4); // dataSize;
	memcpy(((char*) p) + 4, &status, 4); // status;
	memcpy(((char*) p) + 8, &status, 4); // direction;

	packet->message = new char[len];
	strncpy(packet->message, msg, len - 1);
	packet->message[strlen(msg)] = '\0';
	memcpy(((char*) p) + 12, msg, len); // status

	ioInfo->wsaBuf.buf = (char*) p;
	ioInfo->wsaBuf.len = len + (3 * sizeof(int));
	ioInfo->serverMode = WRITE; // GetQueuedCompletionStatus 이후 분기가 Send로 갈수 있게

	WSASend(mySock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);
}

// 같은 방의 사람들에게 메세지 전달
void SendToRoomMsg(char *msg, list<SOCKET> &lists, int status) {

	LPPER_IO_DATA ioInfo = new PER_IO_DATA;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	P_PACKET_DATA packet = new PACKET_DATA;

	int len = strlen(msg) + 1;
	char *p;
	p = new char[len + (3 * sizeof(int))];
	memcpy(p, &len, 4); // dataSize;
	memcpy(((char*) p) + 4, &status, 4); // status;
	memcpy(((char*) p) + 8, &status, 4); // direction;

	packet->message = new char[len];
	strncpy(packet->message, msg, len - 1);
	packet->message[strlen(msg)] = '\0';
	memcpy(((char*) p) + 12, msg, len); // status

	ioInfo->wsaBuf.buf = (char*) p;
	ioInfo->wsaBuf.len = len + (3 * sizeof(int));
	ioInfo->serverMode = WRITE;  // GetQueuedCompletionStatus 이후 분기가 Send로 갈수 있게

	list<SOCKET>::iterator iter;
	EnterCriticalSection(&cs);
	for (iter = lists.begin(); iter != lists.end(); iter++) {
		WSASend((*iter), &(ioInfo->wsaBuf), 1,
		NULL, 0, &(ioInfo->overlapped), NULL);
	}
	LeaveCriticalSection(&cs);

}

// Recv 계속 공통함수
void RecvMore(SOCKET sock, DWORD recvBytes, DWORD flags, LPPER_IO_DATA ioInfo,
		DWORD remainSize) {

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = remainSize;
	memset(ioInfo->buffer, 0, SIZE);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ_MORE; // GetQueuedCompletionStatus 이후 분기가 Recv로 갈수 있게
	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}
// Recv 공통함수
void Recv(SOCKET sock, DWORD recvBytes, DWORD flags) {
	LPPER_IO_DATA ioInfo = new PER_IO_DATA;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = SIZE;
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ; // GetQueuedCompletionStatus 이후 분기가 Recv로 갈수 있게
	ioInfo->recvByte = 0;
	ioInfo->totByte = 0;
	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}

// 초기 로그인
// 세션정보 추가
void InitUser(char *id, SOCKET sock) {

	char msg[BUF_SIZE];
	char name[NAME_SIZE];

	strncpy(name, idMap.find(id)->second.nickname, NAME_SIZE);

	LPPER_HANDLE_DATA handleInfo = new PER_HANDLE_DATA;
	cout << "START user : " << name << endl;
	cout << "sock : " << sock << endl;
	// 유저의 상태 정보 초기화
	// ioInfo = new PER_IO_DATA;
	handleInfo->status = STATUS_WAITING;

	strncpy(handleInfo->userId, id, NAME_SIZE);
	strncpy(handleInfo->userName, name, NAME_SIZE);
	strncpy(handleInfo->roomName, "", NAME_SIZE);
	EnterCriticalSection(&cs);
	// 세션정보 insert
	userMap.insert(pair<SOCKET, LPPER_HANDLE_DATA>(sock, handleInfo));

	// id의 key값은 유저 이름이 아니라 아이디!!
	idMap.find(id)->second.logMode = NOW_LOGIN;

	cout << "현재 접속 인원 수 : " << userMap.size() << endl;

	LeaveCriticalSection(&cs);

	strncpy(msg, "입장을 환영합니다!\n", BUF_SIZE);
	strcat(msg, waitRoomMessage);
	SendToOneMsg(msg, sock, STATUS_WAITING);

}

// 접속 강제종료 로직
void ClientExit(SOCKET sock) {

	if (closesocket(sock) != SOCKET_ERROR) {
		cout << "정상 종료 " << endl;

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
						EnterCriticalSection(&cs);
						roomMap.erase(roomName);
						LeaveCriticalSection(&cs);
					} else {
						char sendMsg[BUF_SIZE];
						// 이름먼저 복사 NAME_SIZE까지
						strncpy(sendMsg, name, NAME_SIZE);
						strcat(sendMsg, " 님이 나갔습니다!");

						if (roomMap.find(userMap.find(sock)->second->roomName)
								!= roomMap.end()) { // null 검사
							SendToRoomMsg(sendMsg,
									roomMap.find(
											userMap.find(sock)->second->roomName)->second,
									STATUS_CHATTIG);
						}
					}
				}
			}
			EnterCriticalSection(&cs);
			userMap.erase(sock); // 접속 소켓 정보 삭제
			LeaveCriticalSection(&cs);
			cout << "현재 접속 인원 수 : " << userMap.size() << endl;
		}

	}

}

// 로그인 이전 로직처리
// 세션값 없을 때 로직
void StatusLogout(SOCKET sock, int status, int direction, char *message) {

	if (status == STATUS_LOGOUT) {

		// memcpy 부분 삭제
		char msg[BUF_SIZE];

		if (direction == USER_MAKE) { // 1번 계정생성
			cout << "USER MAKE" << endl;
			USER_DATA userData;
			char *sArr[3] = { NULL, };
			char *ptr = strtok(message, "\\"); // 공백 문자열을 기준으로 문자열을 자름
			int i = 0;
			while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
			{
				sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
				i++;                       // 인덱스 증가
				ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
			}
			// 유효성 검증 필요
			EnterCriticalSection(&cs);
			if (idMap.find(sArr[0]) == idMap.end()) { // ID 중복체크

				userData.password = string(sArr[1]);
				strncpy(userData.nickname, sArr[2], NAME_SIZE);
				userData.logMode = NOW_LOGOUT;

				idMap.insert(
						pair<string, USER_DATA>(string(sArr[0]), userData));

				strncpy(msg, sArr[0], BUF_SIZE);
				strcat(msg, " 계정 생성 완료!\n");
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(msg, sock, STATUS_LOGOUT);
			} else { // ID중복있음
				strncpy(msg, "중복된 아이디가 있습니다!\n", BUF_SIZE);
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(msg, sock, STATUS_LOGOUT);
			}
			LeaveCriticalSection(&cs);
		} else if (direction == USER_ENTER) { // 2번 로그인 시도
			cout << "USER ENTER" << endl;
			char *sArr[2] = { NULL, };
			char *ptr = strtok(message, "\\"); // 공백 문자열을 기준으로 문자열을 자름
			int i = 0;
			while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
			{
				sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
				i++;                       // 인덱스 증가
				ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
			}

			if (idMap.find(sArr[0]) == idMap.end()) { // 계정 없음
				strncpy(msg, "없는 계정입니다 아이디를 확인하세요!\n", BUF_SIZE);
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(msg, sock, STATUS_LOGOUT);
			} else if (idMap.find(sArr[0])->second.password.compare(
					string(sArr[1])) == 0) { // 비밀번호 일치

				if (idMap.find(sArr[0])->second.logMode == NOW_LOGIN) { // 중복로그인 유효성 검사
					strncpy(msg, "중복 로그인은 안됩니다!\n", BUF_SIZE);
					strcat(msg, loginBeforeMessage);
					SendToOneMsg(msg, sock, STATUS_LOGOUT);
				} else { // 중복로그인 X
					InitUser(sArr[0], sock); // 세션정보 저장
				}

			} else { // 비밀번호 틀림
				strncpy(msg, "비밀번호 틀림!\n", BUF_SIZE);
				strcat(msg, loginBeforeMessage);
				SendToOneMsg(msg, sock, STATUS_LOGOUT);
			}

		} else if (direction == USER_LIST) { // 유저들 정보 요청
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

			SendToOneMsg(userList, sock, STATUS_LOGOUT);
		} else { // 그외 명령어 입력
			char sendMsg[BUF_SIZE];

			strncpy(sendMsg, errorMessage, BUF_SIZE);
			strcat(sendMsg, loginBeforeMessage);
			// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다
			SendToOneMsg(sendMsg, sock, STATUS_LOGOUT);
		}
	}

}

// 대기실에서의 로직 처리
// 세션값 있음
void StatusWait(SOCKET sock, int status, int direction, char *message) {

	char name[BUF_SIZE];
	char msg[BUF_SIZE];
	// 세션에서 이름 정보 리턴
	strncpy(name, userMap.find(sock)->second->userName, BUF_SIZE);
	strncpy(msg, message, BUF_SIZE);
	if (direction == ROOM_MAKE) { // 새로운 방 만들때
		cout << "ROOM MAKE" << endl;
		strncpy(msg, message, BUF_SIZE);
		// 유효성 검증 먼저
		if (roomMap.count(msg) != 0) { // 방이름 중복
			strncpy(msg, "이미 있는 방 이름입니다!\n", BUF_SIZE);
			strcat(msg, waitRoomMessage);
			SendToOneMsg(msg, sock, STATUS_WAITING);
			// 중복 케이스는 방 만들 수 없음
		} else { // 방이름 중복 아닐 때만 개설

			// 새로운 방 정보 저장
			list<SOCKET> chatList;
			chatList.push_back(sock);
			EnterCriticalSection(&cs);
			roomMap.insert(pair<string, list<SOCKET> >(msg, chatList));

			// User의 상태 정보 바꾼다
			strncpy((userMap.find(sock))->second->roomName, msg, NAME_SIZE);
			(userMap.find(sock))->second->status =
			STATUS_CHATTIG;

			LeaveCriticalSection(&cs);
			strcat(msg, " 방이 개설되었습니다.");
			strcat(msg, chatRoomMessage);
			cout << "현재 서버의 방 갯수 : " << roomMap.size() << endl;
			SendToOneMsg(msg, sock, STATUS_CHATTIG);
		}

	} else if (direction == ROOM_ENTER) { // 방 입장 요청
		cout << "ROOM ENTER" << endl;

		// 유효성 검증 먼저
		if (roomMap.count(msg) == 0) { // 방 못찾음
			strncpy(msg, "없는 방 입니다!\n", sizeof("없는 방 입니다!\n"));
			strcat(msg, waitRoomMessage);
			SendToOneMsg(msg, sock, STATUS_WAITING);
		} else {
			EnterCriticalSection(&cs);

			if (roomMap.find(msg) != roomMap.end()) {
				roomMap.find(msg)->second.push_back(sock);
			}

			strncpy(userMap.find(sock)->second->roomName, msg, NAME_SIZE);
			userMap.find(sock)->second->status = STATUS_CHATTIG;

			LeaveCriticalSection(&cs);

			char sendMsg[BUF_SIZE];
			strncpy(sendMsg, name, BUF_SIZE);
			strcat(sendMsg, " 님이 입장하셨습니다. ");
			strcat(sendMsg, chatRoomMessage);

			if (roomMap.find(msg) != roomMap.end()) { // 안전성 검사

				SendToRoomMsg(sendMsg, roomMap.find(msg)->second,
				STATUS_CHATTIG);

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

		SendToOneMsg(str, sock, STATUS_WAITING);
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
		SendToOneMsg(str, sock, STATUS_WAITING);
	} else if (direction == WHISPER) { // 귓속말
		cout << "WHISPER " << endl;
		char *sArr[2] = { NULL, };
		char *ptr = strtok(message, "\\"); // 공백 문자열을 기준으로 문자열을 자름
		int i = 0;
		while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		strncpy(name, sArr[0], NAME_SIZE);
		strncpy(msg, sArr[1], NAME_SIZE);

		map<SOCKET, LPPER_HANDLE_DATA>::iterator iter;
		char sendMsg[BUF_SIZE];

		if (strcmp(name, userMap.find(sock)->second->userName) == 0) { // 본인에게 쪽지
			strncpy(sendMsg, "자신에게 쪽지를 보낼수 없습니다\n", BUF_SIZE);
			strcat(sendMsg, waitRoomMessage);
			SendToOneMsg(sendMsg, sock, STATUS_WAITING);
		} else {
			bool find = false;
			EnterCriticalSection(&cs);
			for (iter = userMap.begin(); iter != userMap.end(); iter++) {
				if (strcmp(iter->second->userName, name) == 0) {
					find = true;

					strncpy(sendMsg, userMap.find(sock)->second->userName,
					BUF_SIZE);
					strcat(sendMsg, " 님에게 온 귓속말 : ");
					strcat(sendMsg, msg);
					SendToOneMsg(sendMsg, iter->first, STATUS_WHISPER);
					break;
				}
			}
			LeaveCriticalSection(&cs);
			// 귓속말 대상자 못찾음
			if (!find) {
				strncpy(sendMsg, name, BUF_SIZE);
				strcat(sendMsg, " 님을 찾을 수 없습니다");
				SendToOneMsg(sendMsg, sock, STATUS_WAITING);
			}
		}

	} else if (strcmp(msg, "6") == 0) { // 로그아웃
		EnterCriticalSection(&cs);

		string userId = idMap.find(userMap.find(sock)->second->userId)->first;
		userMap.erase(sock); // 접속 소켓 정보 삭제
		idMap.find(userId)->second.logMode = NOW_LOGOUT;
		LeaveCriticalSection(&cs);
		char sendMsg[BUF_SIZE];

		strncpy(sendMsg, loginBeforeMessage, BUF_SIZE);
		SendToOneMsg(sendMsg, sock, STATUS_LOGOUT);
	} else { // 그외 명령어 입력
		char sendMsg[BUF_SIZE];

		strncpy(sendMsg, errorMessage, BUF_SIZE);
		strcat(sendMsg, waitRoomMessage);
		// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다
		SendToOneMsg(sendMsg, sock, STATUS_WAITING);
	}
}

// 채팅방에서의 로직 처리
// 세션값 있음
void StatusChat(SOCKET sock, int status, int direction, char *message) {

	char name[NAME_SIZE];
	char msg[BUF_SIZE];

	// 세션에서 이름 정보 리턴
	strncpy(name, userMap.find(sock)->second->userName, NAME_SIZE);
	strncpy(msg, message, BUF_SIZE);

	if (strcmp(msg, "\\out") == 0) { // 채팅방 나감

		char sendMsg[BUF_SIZE];
		char roomName[NAME_SIZE];
		strncpy(sendMsg, name, BUF_SIZE);
		strcat(sendMsg, " 님이 나갔습니다!");

		if (roomMap.find(userMap.find(sock)->second->roomName)
				!= roomMap.end()) { // null 검사

			SendToRoomMsg(sendMsg,
					roomMap.find(userMap.find(sock)->second->roomName)->second,
					STATUS_CHATTIG);

			strncpy(roomName, userMap.find(sock)->second->roomName,
			NAME_SIZE);
			// 방이름 임시 저장
			strncpy(userMap.find(sock)->second->roomName, "", NAME_SIZE);
			userMap.find(sock)->second->status = STATUS_WAITING;
		}

		strncpy(msg, waitRoomMessage, BUF_SIZE);
		SendToOneMsg(msg, sock, STATUS_WAITING);
		cout << "나가는 사람 name : " << name << endl;
		cout << "나가는 방 name : " << roomName << endl;

		// 나가는 사람 정보 out
		EnterCriticalSection(&cs);
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

		if (roomMap.find(userMap.find(sock)->second->roomName)
				!= roomMap.end()) { // null 검사

			SendToRoomMsg(sendMsg,
					roomMap.find(userMap.find(sock)->second->roomName)->second,
					STATUS_CHATTIG);
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

	while (true) {

		bool success = GetQueuedCompletionStatus(hComPort, &bytesTrans,
				(LPDWORD) &sock, (LPOVERLAPPED*) &ioInfo, INFINITE);

		if (bytesTrans == 0 && !success) { // 접속 끊김 콘솔 강제 종료
			// 콘솔 강제종료 처리
			ClientExit(sock);

		} else if (READ == ioInfo->serverMode
				|| READ_MORE == ioInfo->serverMode) { // Recv 끝난경우

				// IO 완료후 동작 부분
			if (READ_MORE != ioInfo->serverMode) {
				memcpy(&(ioInfo->bodySize), ioInfo->buffer, 4);
				ioInfo->recvBuffer = new char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당
				memcpy(((char*) ioInfo->recvBuffer), ioInfo->buffer,
						bytesTrans);

			} else {
				if (ioInfo->recvByte > 12) {
					memcpy(((char*) ioInfo->recvBuffer) + ioInfo->recvByte,
							ioInfo->buffer, bytesTrans);
				}
			}
			if (ioInfo->totByte == 0) {
				ioInfo->totByte = ioInfo->bodySize + 12;
			}
			ioInfo->recvByte += bytesTrans;
			if (ioInfo->recvByte < ioInfo->totByte) { // 받은 패킷 부족 -> 더받아야함
				DWORD recvBytes = 0;
				DWORD flags = 0;
				RecvMore(sock, recvBytes, flags, ioInfo,
						ioInfo->totByte - ioInfo->recvByte); // 패킷 더받기
			} else {

				DWORD recvBytes = 0;
				DWORD flags = 0;
				int status;
				int direction;
				memcpy(&status, ((char*) ioInfo->recvBuffer) + 4, 4);
				memcpy(&direction, ((char*) ioInfo->recvBuffer) + 8, 4);
				char *msg = new char[ioInfo->bodySize];
				memcpy(msg, ((char*) ioInfo->recvBuffer) + 12,
						ioInfo->bodySize);
				if (userMap.find(sock) == userMap.end()) { // 세션값 없음 => 로그인 이전 분기
						// 로그인 이전 로직 처리
					StatusLogout(sock, status, direction, msg);
					// Recv는 계속한다
					Recv(sock, recvBytes, flags);
					// 세션값 없음 => 로그인 이전 분기 끝
				} else { // 세션값 있을때 => 대기방 또는 채팅방 상태

					int status = userMap.find(sock)->second->status;

					if (status == STATUS_WAITING) { // 대기실 케이스
						// 대기실 처리 함수
						StatusWait(sock, status, direction, msg);
					} else if (status == STATUS_CHATTIG) { // 채팅 중 케이스
						// 채팅방 처리 함수
						StatusChat(sock, status, direction, msg);
					}

					// Recv는 계속한다
					Recv(sock, recvBytes, flags);
				}
			}

		} else if (WRITE == ioInfo->serverMode) { // Send 끝난경우
			cout << "message send" << endl;
		}
	}
	return 0;
}

int main(int argc, char* argv[]) {

	WSADATA wsaData;
	HANDLE hComPort;
	SYSTEM_INFO sysInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAdr;
	DWORD recvBytes = 0;
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
	// Thread Pool 스레드를 필요한 만큼 만들어 놓고 파괴 안하고 사용
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

		cout << "Connected client IP " << inet_ntoa(clntAdr.sin_addr) << endl;

		// Completion Port 와 accept한 소켓 연결
		CreateIoCompletionPort((HANDLE) hClientSock, hComPort,
				(DWORD) hClientSock, 0);

		Recv(hClientSock, recvBytes, flags);

		// 초기 접속 메세지 Send
		char msg[BUF_SIZE];
		strncpy(msg, "접속을 환영합니다!\n", BUF_SIZE);
		strcat(msg, loginBeforeMessage);
		SendToOneMsg(msg, hClientSock, STATUS_LOGOUT);

	}

	// 임계영역 Object 반환
	DeleteCriticalSection(&cs);
	return 0;
}
