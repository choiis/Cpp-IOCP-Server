//============================================================================
// Name        : Client.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>
#include <regex>
#include "common.h"
#include "MPool.h"
#include "CharPool.h"
// 현재 클라이언트 상태 => 대기실 => 추후 로그인 이전으로 바뀔것
int clientStatus;

CRITICAL_SECTION cs;

using namespace std;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// Recv 계속 공통함수
void RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo) {
	DWORD recvBytes = 0;
	DWORD flags = 0;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = SIZE;
	memset(ioInfo->buffer, 0, SIZE);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ_MORE; // GetQueuedCompletionStatus 이후 분기가 Recv로 갈수 있게

	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}
// Recv 공통함수
void Recv(SOCKET sock) {
	DWORD recvBytes = 0;
	DWORD flags = 0;
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->malloc();

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

// WSASend를 call
void SendMsg(SOCKET clientSock, const char* msg, int status, int direction) {
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

	int len = strlen(msg) + 1;
	CharPool* charPool = CharPool::getInstance();
	char* packet = charPool->malloc(); // char[len + (3 * sizeof(int))];
	memcpy(packet, &len, 4); // dataSize;
	memcpy(((char*) packet) + 4, &status, 4); // status;
	memcpy(((char*) packet) + 8, &direction, 4); // direction;
	memcpy(((char*) packet) + 12, msg, len); // status;

	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = len + (3 * sizeof(int));
	ioInfo->serverMode = WRITE;
	// cout << "send " << msg << " status " << status << " direction " << direction << endl;

	WSASend(clientSock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped),
	NULL);
}

string alpha1 = "abcdefghijklmnopqrstuvwxyz";
string alpha2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
string password = "1234";

// 송신을 담당할 스레드
unsigned WINAPI SendMsgThread(void *arg) {
	// 넘어온 clientSocket을 받아줌
	SOCKET clientSock = *((SOCKET*) arg);

	while (1) {

		int direction = -1;
		int status = -1;
		Sleep(1);
		EnterCriticalSection(&cs);
		if (clientStatus == STATUS_LOGOUT) { // 로그인 이전
			LeaveCriticalSection(&cs);
			int randNum3 = (rand() % 2);
			if (randNum3 % 2 == 0) {
				int randNum1 = (rand() % 2);
				int randNum2 = (rand() % 26);
				string nickName = "";
				if (randNum1 == 0) {
					nickName += alpha1.at(randNum2);
				} else {
					nickName += alpha2.at(randNum2);
				}
				string msg1 = nickName;
				msg1.append("\\");
				msg1.append(password); // 비밀번호
				msg1.append("\\");
				msg1.append(nickName); // 닉네임
				direction = USER_MAKE;
				status = STATUS_LOGOUT;

				SendMsg(clientSock, msg1.c_str(), status, direction);
			} else {
				int randNum1 = (rand() % 2);
				int randNum2 = (rand() % 26);
				string nickName = "";
				if (randNum1 == 0) {
					nickName += alpha1.at(randNum2);
				} else {
					nickName += alpha2.at(randNum2);
				}
				string msg2 = nickName;
				msg2.append("\\");
				msg2.append(password); // 비밀번호
				direction = USER_ENTER;
				status = STATUS_LOGOUT;
				SendMsg(clientSock, msg2.c_str(), status, direction);
			}

		} else if (clientStatus == STATUS_WAITING) {
			LeaveCriticalSection(&cs);

			int directionNum = (rand() % 10);
			if ((directionNum % 2) == 0) { // 0 2 4 6 8 방입장

				int randNum1 = (rand() % 2);
				int randNum2 = (rand() % 4);
				string roomName = "";
				if (randNum1 == 0) {
					roomName += alpha1.at(randNum2);
				} else {
					roomName += alpha2.at(randNum2);
				}
				direction = ROOM_ENTER;
				status = STATUS_WAITING;
				SendMsg(clientSock, roomName.c_str(), status, direction);
			} else if ((directionNum % 3) == 0) { // 3 6 9 방만들기
				int randNum1 = (rand() % 2);
				int randNum2 = (rand() % 4);
				string roomName = "";
				if (randNum1 == 0) {
					roomName += alpha1.at(randNum2);
				} else {
					roomName += alpha2.at(randNum2);
				}
				direction = ROOM_MAKE;
				status = STATUS_WAITING;
				SendMsg(clientSock, roomName.c_str(), status, direction);
			} else if (directionNum == 7) { // 7 방정보
				direction = ROOM_INFO;
				status = STATUS_WAITING;
				SendMsg(clientSock, "", status, direction);
			} else if (directionNum == 1) { // 1 유저정보
				direction = ROOM_USER_INFO;
				status = STATUS_WAITING;
				SendMsg(clientSock, "", status, direction);
			}

		} else if (clientStatus == STATUS_CHATTIG) {
			LeaveCriticalSection(&cs);
			int directionNum = (rand() % 20);
			string msg = "";
			if (directionNum < 18) {
				int randNum1 = (rand() % 2);

				for (int i = 0; i <= (rand() % 60) + 1; i++) {
					int randNum2 = (rand() % 26);
					if (randNum1 == 0) {
						msg += alpha1.at(randNum2);
					} else {
						msg += alpha2.at(randNum2);
					}
				}

			} else if (directionNum == 18) { // 20번에 한번 clear
				system("cls");
			} else { // 20번에 한번 나감
				msg = "\\out";
			}
			status = STATUS_CHATTIG;
			SendMsg(clientSock, msg.c_str(), status, direction);
		} else {
			LeaveCriticalSection(&cs);
		}

	}
	return 0;
}

// 패킷 데이터 읽기
void PacketReading(LPPER_IO_DATA ioInfo, DWORD bytesTrans) {
	// IO 완료후 동작 부분
	if (READ == ioInfo->serverMode) {
		if (bytesTrans < 4) { // 헤더를 다 못 읽어온 상황
			memcpy(((char*) &(ioInfo->bodySize)) + ioInfo->recvByte,
					ioInfo->buffer, bytesTrans);
		} else {
			memcpy(&(ioInfo->bodySize), ioInfo->buffer, 4);
			CharPool* charPool = CharPool::getInstance();
			ioInfo->recvBuffer = charPool->malloc(); // char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당
			memcpy(((char*) ioInfo->recvBuffer), ioInfo->buffer, bytesTrans);
		}
		ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
	} else { // 더 읽기
		if (ioInfo->recvByte >= 4) { // 헤더 다 읽었음
			memcpy(((char*) ioInfo->recvBuffer) + ioInfo->recvByte,
					ioInfo->buffer, bytesTrans);
			ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
		} else { // 헤더를 다 못읽었음 경우
			int recv = min(4 - ioInfo->recvByte, bytesTrans);
			memcpy(((char*) &(ioInfo->bodySize)) + ioInfo->recvByte,
					ioInfo->buffer, recv); // 헤더부터 채운다
			ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
			if (ioInfo->recvByte >= 4) {
				CharPool* charPool = CharPool::getInstance();
				ioInfo->recvBuffer = charPool->malloc(); // char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당
				memcpy(((char*) ioInfo->recvBuffer) + 4,
						((char*) ioInfo->buffer) + recv, bytesTrans - recv); // 이제 헤더는 필요없음 => 이때는 뒤의 데이터만 읽자
			}
		}
	}

	if (ioInfo->totByte == 0 && ioInfo->recvByte >= 4) { // 헤더를 다 읽어야 토탈 바이트 수를 알 수 있다
		ioInfo->totByte = ioInfo->bodySize + 12;
	}
}

// 클라이언트에게 받은 데이터 복사후 구조체 해제
char* DataCopy(LPPER_IO_DATA ioInfo, int *status) {
	memcpy(status, ((char*) ioInfo->recvBuffer) + 4, 4); // Status
	CharPool* charPool = CharPool::getInstance();
	char* msg = charPool->malloc(); // char[ioInfo->bodySize];	// Msg
	memcpy(msg, ((char*) ioInfo->recvBuffer) + 12, ioInfo->bodySize);
	// 다 복사 받았으니 할당 해제
	charPool->free(ioInfo->recvBuffer);
	// 메모리 해제
	MPool* mp = MPool::getInstance();
	mp->free(ioInfo);

	return msg;
}

// 수신을 담당할 스레드
unsigned WINAPI RecvMsgThread(LPVOID hComPort) {

	SOCKET sock;
	DWORD bytesTrans;
	LPPER_IO_DATA ioInfo;

	while (1) {
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &sock,
				(LPOVERLAPPED*) &ioInfo, INFINITE);

		if (READ_MORE == ioInfo->serverMode || READ == ioInfo->serverMode) {
			// 데이터 읽기 과정
			PacketReading(ioInfo, bytesTrans);

			if ((ioInfo->recvByte < ioInfo->totByte)
					|| (ioInfo->recvByte < 4 && ioInfo->totByte == 0)) { // 받은 패킷 부족 || 헤더 다 못받음 -> 더받아야함

				RecvMore(sock, ioInfo); // 패킷 더받기
			} else {
				int status;
				char *msg = DataCopy(ioInfo, &status);

				// Client의 상태 정보 갱신 필수
				// 서버에서 준것으로 갱신
				if (status == STATUS_LOGOUT || status == STATUS_WAITING
						|| status == STATUS_CHATTIG) {
					EnterCriticalSection(&cs);
					if (clientStatus != status) { // 상태 변경시 콘솔 clear
						system("cls");
					}
					clientStatus = status; // clear이후 client상태변경 해준다
					LeaveCriticalSection(&cs);
					cout << msg << endl;
				} else if (status == STATUS_WHISPER) { // 귓속말 상태일때는 클라이언트 상태변화 없음
					cout << msg << endl;
				}
				CharPool* charPool = CharPool::getInstance();
				charPool->free(msg);
				Recv(sock);
			}
		} else if (WRITE == ioInfo->serverMode) {
			MPool* mp = MPool::getInstance();
			CharPool* charPool = CharPool::getInstance();
			charPool->free(ioInfo->wsaBuf.buf);
			mp->free(ioInfo);
		}

	}
	return 0;
}
int main() {

	WSADATA wsaData;
	SOCKET hSocket;
	SOCKADDR_IN servAddr;

	HANDLE sendThread, recvThread;

	// Socket lib의 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup() error!");
		exit(1);
	}
	// Overlapped IO가능 소켓을 만든다
	// TCP 통신할것
	hSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
	WSA_FLAG_OVERLAPPED);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = PF_INET;
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	// 임계영역 Object 생성
	InitializeCriticalSectionAndSpinCount(&cs, 2000);
	while (1) {

		cout << "포트번호를 입력해 주세요 :";
		string port;
		cin >> port;

		servAddr.sin_port = htons(atoi(port.c_str()));

		if (connect(hSocket, (SOCKADDR*) &servAddr,
				sizeof(servAddr))==SOCKET_ERROR) {
			printf("connect() error!");
		} else {
			break;
		}
	}
	// Completion Port 생성
	HANDLE hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// Completion Port 와 소켓 연결
	CreateIoCompletionPort((HANDLE) hSocket, hComPort, (DWORD) hSocket, 0);

	// 수신 스레드 동작
	// 만들어진 RecvMsg를 hComPort CP 오브젝트에 할당한다
	// RecvMsg에서 Recv가 완료되면 동작할 부분이 있다
	recvThread = (HANDLE) _beginthreadex(NULL, 0, RecvMsgThread,
			(LPVOID) hComPort, 0,
			NULL);

	// 송신 스레드 동작
	// Thread안에서 clientSocket으로 Send해줄거니까 인자로 넘겨준다
	// CP랑 Send는 연결 안되어있음 GetQueuedCompletionStatus에서 Send 완료처리 필요없음
	sendThread = (HANDLE) _beginthreadex(NULL, 0, SendMsgThread,
			(void*) &hSocket, 0,
			NULL);

	Recv(hSocket);
	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	// 임계영역 Object 반환
	DeleteCriticalSection(&cs);
	closesocket(hSocket);
	WSACleanup();
	return 0;
}
