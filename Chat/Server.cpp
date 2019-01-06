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
#include <ws2tcpip.h>
#include <map>
#include <list>

using namespace std;

#define BUF_SIZE 512
#define NAME_SIZE 20

#define START 1
#define READ 2
#define WRITE 3

// 클라이언트 상태 정보 => 서버에서 보관할것
#define STATUS_LOGOUT 1
#define STATUS_WAITING 2
#define STATUS_CHATTIG 3

// 서버에 올 지시 사항
#define ROOM_MAKE 1
#define ROOM_ENTER 2
#define ROOM_OUT 3

// 서버에 접속한 유저 정보
// client 소켓에 대응하는 세션정보
typedef struct { // socket info
	char userName[NAME_SIZE];
	char roomName[NAME_SIZE];
	int status;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// 실제 통신 패킷 데이터
typedef struct {
	int clientStatus;
	char message[BUF_SIZE];
	int direction;
} PACKET_DATA, *P_PACKET_DATA;

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[sizeof(PACKET_DATA)];
	int serverMode;
} PER_IO_DATA, *LPPER_IO_DATA;

// 서버에 접속한 유저 자료 저장
map<SOCKET, LPPER_HANDLE_DATA> userMap;
// 서버의 방 정보 저장
map<string, list<SOCKET> > roomMap;

// 임계영역에 필요한 오브젝트
// WaitForSingleObject는 mutex를 non-singled로 만듬 => 시작
// ReleaseMutex는  mutex를 singled로 만듬 => 끝
HANDLE mutex;

// 본인에게 메세지 전달
void SendToMeMsg(LPPER_IO_DATA ioInfo, char *msg, SOCKET mySock, int status) {

	// cout << "SendToMeMsg " << msg << endl;

	ioInfo = new PER_IO_DATA;

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	P_PACKET_DATA packet = new PACKET_DATA;

	strcpy(packet->message, msg);
	packet->clientStatus = status;
	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
	ioInfo->serverMode = WRITE;

	WSASend(mySock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);

	delete ioInfo;
	// delete packet;
}

// 같은 방의 사람들에게 메세지 전달
void SendToRoomMsg(LPPER_IO_DATA ioInfo, char *msg, list<SOCKET> lists,
		int status) {

	// cout << "SendToRoomMsg " << msg << endl;

	ioInfo = new PER_IO_DATA;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	P_PACKET_DATA packet = new PACKET_DATA;

	strcpy(packet->message, msg);
	packet->clientStatus = status;
	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
	ioInfo->serverMode = WRITE;

	WaitForSingleObject(mutex, INFINITE);
	list<SOCKET>::iterator iter;
	for (iter = lists.begin(); iter != lists.end(); iter++) {
		WSASend((*iter), &(ioInfo->wsaBuf), 1,
		NULL, 0, &(ioInfo->overlapped), NULL);
	}
	ReleaseMutex(mutex);

	delete ioInfo;
	// delete packet;
}

// 전체 사람들에게 메세지 전달
// 사자후 기능 추후 구현 예정
void SendToAllMsg(LPPER_IO_DATA ioInfo, char *msg, SOCKET mySock, int status) {

	cout << "SendToAllMsg " << msg << endl;

	ioInfo = new PER_IO_DATA;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	P_PACKET_DATA packet = new PACKET_DATA;

	strcpy(packet->message, msg);
	packet->clientStatus = status;
	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
	ioInfo->serverMode = WRITE;

	WaitForSingleObject(mutex, INFINITE);
	map<SOCKET, LPPER_HANDLE_DATA>::iterator iter;
	for (iter = userMap.begin(); iter != userMap.end(); iter++) {
		if (iter->first == mySock) { // 본인 제외
			continue;
		} else {
			WSASend(iter->first, &(ioInfo->wsaBuf), 1, NULL, 0,
					&(ioInfo->overlapped), NULL);
		}
	}
	ReleaseMutex(mutex);

	// delete packet;
	delete ioInfo;
}

// 초기 접속
void InitUser(P_PACKET_DATA packet, LPPER_IO_DATA ioInfo, SOCKET sock) {

	char msg[BUF_SIZE];
	char name[NAME_SIZE];
	DWORD flags = 0;
	packet = new PACKET_DATA;

	memcpy(packet, ioInfo->buffer, sizeof(PACKET_DATA));
	strcpy(name, packet->message);
	LPPER_HANDLE_DATA handleInfo = new PER_HANDLE_DATA;
	cout << "START user : " << name << endl;
	cout << "sock : " << sock << endl;
	// 유저의 상태 정보 초기화
	ioInfo = new PER_IO_DATA;
	handleInfo->status = STATUS_WAITING;
	strcpy(handleInfo->userName, name);
	strcpy(handleInfo->roomName, "");
	WaitForSingleObject(mutex, INFINITE);
	userMap.insert(pair<SOCKET, LPPER_HANDLE_DATA>(sock, handleInfo));
	cout << "현재 접속 인원 수 : " << userMap.size() << endl;

	// map<SOCKET, LPPER_HANDLE_DATA>::iterator iter;
	// for (iter = userMap.begin(); iter != userMap.end(); ++iter) {
	// 	cout << "접속자 이름 : " << (*iter).second->userName << (*iter).first
	//	 		<< endl;
	// }

	ReleaseMutex(mutex);

	delete ioInfo;

	strcpy(msg, "입장을 환영합니다!\n");
	strcat(msg, "1.방 정보 보기, 2.방 만들기 3.방 입장하기");
	SendToMeMsg(ioInfo, msg, sock, STATUS_WAITING);

	ioInfo = new PER_IO_DATA;

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ;

	WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags, &(ioInfo->overlapped),
	NULL);

}

// Server 컴퓨터  CPU 개수만큼 스레드 생성될것
unsigned WINAPI HandleThread(LPVOID pCompPort) {

	HANDLE hComPort = (HANDLE) pCompPort;
	SOCKET sock;
	DWORD bytesTrans;
	// LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;

	P_PACKET_DATA packet;

	while (true) {

		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &sock,
				(LPOVERLAPPED*) &ioInfo, INFINITE);

		if (START == ioInfo->serverMode) {

			InitUser(packet, ioInfo, sock);

		} else if (READ == ioInfo->serverMode) {
			cout << "message received" << endl;

			// IO 완료후 동작 부분
			char name[BUF_SIZE];
			char msg[BUF_SIZE];
			DWORD flags = 0;
			int direction = 0;

			// 패킷을 받고 이름과 메세지를 초기 저장
			packet = new PACKET_DATA;
			memcpy(packet, ioInfo->buffer, sizeof(PACKET_DATA));

			strcpy(name, userMap.find(sock)->second->userName);
			strcpy(msg, packet->message);

			if (bytesTrans == 0 || strcmp(ioInfo->buffer, "out") == 0) { // 접속 끊김
				char roomName[NAME_SIZE];

				strcpy(roomName, userMap.find(sock)->second->roomName);
				// 방이름 임시 저장
				if (strcmp(roomName, "") != 0) { // 방에 접속중인 경우
					// 나가는 사람 정보 out
					(roomMap.find(roomName)->second).remove(sock);

					if ((roomMap.find(roomName)->second).size() == 0) { // 방인원 0명이면 방 삭제
						roomMap.erase(roomName);
					} else {
						char sendMsg[NAME_SIZE + BUF_SIZE + 4];
						strcpy(sendMsg, name);
						strcat(sendMsg, " 님이 나갔습니다!");

						SendToRoomMsg(ioInfo, sendMsg,
								roomMap.find(
										userMap.find(sock)->second->roomName)->second,
								STATUS_CHATTIG);
					}
				}

				WaitForSingleObject(mutex, INFINITE);
				userMap.erase(sock); // 접속 소켓 정보 삭제
				cout << "현재 접속 인원 수 : " << userMap.size() << endl;
				ReleaseMutex(mutex);
				closesocket(sock);

				// delete ioInfo;
				continue;
			}

			int nowStatus = (userMap.find(sock))->second->status;
			direction = packet->direction;

			if (nowStatus == STATUS_WAITING) { // 대기실 케이스
				if (direction == ROOM_MAKE) { // 새로운 방 만들때
					// cout << "ROOM_MAKE" << endl;

					// 유효성 검증 먼저
					if (roomMap.count(msg) != 0) { // 방이름 중복
						strcpy(msg, "이미 있는 방 이름입니다!\n");
						strcat(msg, "1.방 정보 보기, 2.방 만들기 3.방 입장하기");
						SendToMeMsg(ioInfo, msg, sock, STATUS_WAITING);
						// 중복 케이스는 방 만들 수 없음
					} else { // 방이름 중복 아닐 때만 개설

						WaitForSingleObject(mutex, INFINITE);
						// 새로운 방 정보 저장
						list<SOCKET> chatList;
						chatList.push_back(sock);
						roomMap.insert(
								pair<string, list<SOCKET> >(msg, chatList));

						// User의 상태 정보 바꾼다
						strcpy((userMap.find(sock))->second->roomName, msg);
						(userMap.find(sock))->second->status = STATUS_CHATTIG;

						ReleaseMutex(mutex);
						strcat(msg, " 방이 개설되었습니다. 나가시려면 out을 입력하세요");
						cout << "현재 서버의 방 갯수 : " << roomMap.size() << endl;
						SendToMeMsg(ioInfo, msg, sock, STATUS_CHATTIG);
					}

				} else if (direction == ROOM_ENTER) { // 방 입장 요청
					// cout << "ROOM_ENTER" << endl;

					// 유효성 검증 먼저
					if (roomMap.count(msg) == 0) { // 방 못찾음
						strcpy(msg, "없는 방 입니다!\n");
						strcat(msg, "1.방 정보 보기, 2.방 만들기 3.방 입장하기");
						SendToMeMsg(ioInfo, msg, sock, STATUS_WAITING);
					} else {
						WaitForSingleObject(mutex, INFINITE);

						roomMap.find(msg)->second.push_back(sock);
						strcpy(userMap.find(sock)->second->roomName, msg);
						userMap.find(sock)->second->status = STATUS_CHATTIG;

						ReleaseMutex(mutex);

						char sendMsg[NAME_SIZE + BUF_SIZE + 4];
						strcpy(sendMsg, name);
						strcat(sendMsg, " 님이 입장하셨었습니다 나가시려면 out을 입력하세요");
						SendToRoomMsg(ioInfo, sendMsg,
								roomMap.find(msg)->second,
								STATUS_CHATTIG);
					}

				} else if (strcmp(msg, "1") == 0) { // 방 정보 요청시

					WaitForSingleObject(mutex, INFINITE);
					map<string, list<SOCKET> >::iterator iter;
					char str[BUF_SIZE];
					if (roomMap.size() == 0) {
						strcpy(str, "만들어진 방이 없습니다\n");
					} else {
						strcpy(str, "방 정보 리스트\n");
						// 방정보를 문자열로 만든다
						for (iter = roomMap.begin(); iter != roomMap.end();
								iter++) {
							char num[2];
							strcat(str, (iter->first).c_str());
							strcat(str, " : ");
							sprintf(num, "%d", (iter->second).size());
							strcat(str, num);
							strcat(str, " 명 ");
							strcat(str, "\n");
						}
					}
					ReleaseMutex(mutex);
					SendToMeMsg(ioInfo, str, sock, STATUS_WAITING);
				}
			} else if (nowStatus == STATUS_CHATTIG) { // 채팅 중 케이스

				if (bytesTrans == 0 || strcmp(msg, "out") == 0
						|| strcmp(msg, "Q") == 0 || strcmp(msg, "q") == 0) { // 채팅방 나감
						// cout << "채팅방 나가기 " << endl;

					char sendMsg[NAME_SIZE + BUF_SIZE + 4];
					strcpy(sendMsg, name);
					strcat(sendMsg, " 님이 나갔습니다!");

					SendToRoomMsg(ioInfo, sendMsg,
							roomMap.find(userMap.find(sock)->second->roomName)->second,
							STATUS_CHATTIG);
					char roomName[NAME_SIZE];
					// 임계영역 제거해도 될것 같음
					WaitForSingleObject(mutex, INFINITE);

					strcpy(roomName, userMap.find(sock)->second->roomName);
					// 방이름 임시 저장
					strcpy(userMap.find(sock)->second->roomName, "");
					userMap.find(sock)->second->status = STATUS_WAITING;

					ReleaseMutex(mutex);
					// 임계영역 제거해도 될것 같음

					strcpy(msg, "1.방 정보 보기, 2.방 만들기 3.방 입장하기");
					SendToMeMsg(ioInfo, msg, sock, STATUS_WAITING);
					cout << "나가는 사람 name : " << name << endl;
					cout << "나가는 방 name : " << roomName << endl;

					// 나가는 사람 정보 out
					(roomMap.find(roomName)->second).remove(sock);

					if ((roomMap.find(roomName)->second).size() == 0) { // 방인원 0명이면 방 삭제
						roomMap.erase(roomName);
					}
				} else { // 채팅방에서 채팅중
					cout << "채팅방에 있는중 " << endl;
					char sendMsg[NAME_SIZE + BUF_SIZE + 4];
					strcpy(sendMsg, name);
					strcat(sendMsg, " : ");
					strcat(sendMsg, msg);
					SendToRoomMsg(ioInfo, sendMsg,
							roomMap.find(userMap.find(sock)->second->roomName)->second,
							STATUS_CHATTIG);
				}

			}

			delete ioInfo;

			ioInfo = new PER_IO_DATA;
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->serverMode = READ;

			// 계속 Recv
			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags,
					&(ioInfo->overlapped), NULL);
		} else {
			cout << "message send" << endl;

		}
	}
	return 0;
}

int main(int argc, char* argv[]) {

	WSADATA wsaData;
	HANDLE hComPort;
	SYSTEM_INFO sysInfo;
	LPPER_IO_DATA ioInfo;
	LPPER_HANDLE_DATA handleInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAdr;
	int recvBytes, i, flags = 0;
	if (argc != 2) {
		cout << "Usage : " << argv[0] << endl;
		exit(1);
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup() error!");
		exit(1);
	}

	// Completion Port 생성
	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);

	// CPU 갯수 만큼 스레드 생성
	int process = sysInfo.dwNumberOfProcessors;
	cout << "Server CPU num : " << process << endl;
	for (i = 0; i < process; i++) {
		_beginthreadex(NULL, 0, HandleThread, (LPVOID) hComPort, 0, NULL);
	}
	// Overlapped IO가능 소켓을 만든다
	// TCP 통신할것
	hServSock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
	WSA_FLAG_OVERLAPPED);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = PF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(atoi(argv[1]));

	if (bind(hServSock, (SOCKADDR*) &servAdr, sizeof(servAdr)) == SOCKET_ERROR) {
		printf("bind() error!");
		exit(1);
	}

	if (listen(hServSock, 5) == SOCKET_ERROR) {
		printf("listen() error!");
		exit(1);
	}

	// 임계영역 Object 생성
	mutex = CreateMutex(NULL, FALSE, NULL);
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

		// Completion Port 와 소켓 연결
		CreateIoCompletionPort((HANDLE) hClientSock, hComPort,
				(DWORD) hClientSock, 0);

		ioInfo = new PER_IO_DATA;
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

		ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->serverMode = START;

		WSARecv(hClientSock, &(ioInfo->wsaBuf), 1, (LPDWORD) &recvBytes,
				(LPDWORD) &flags, &(ioInfo->overlapped),
				NULL);

		delete handleInfo;
	}

	// 임계영역 Object 반환
	CloseHandle(mutex);
	return 0;
}
