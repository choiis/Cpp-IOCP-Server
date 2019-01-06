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

#define BUF_SIZE 512
#define NAME_SIZE 20

// 클라이언트 상태 정보
#define STATUS_LOGOUT 1
#define STATUS_WAITING 2
#define STATUS_CHATTIG 3

// 서버에게 지시 사항
#define ROOM_MAKE 1
#define ROOM_ENTER 2
#define ROOM_OUT 3

// 현재 클라이언트 상태 => 대기실 => 추후 로그인 이전으로 바뀔것
int clientStatus = 2;

char name[NAME_SIZE] = "";
char msg[BUF_SIZE];

using namespace std;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
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
	int clientMode;
} PER_IO_DATA, *LPPER_IO_DATA;

// 송신을 담당할 스레드
unsigned WINAPI SendMsg(void *arg) {
	SOCKET hSock = *((SOCKET*) arg);
	char msg[BUF_SIZE];

	while (1) {
		gets(msg);
		if (strcmp(msg, "\n") == 0) { // 입력 안하면 반응 없음
			continue;
		} else if (clientStatus == STATUS_WAITING // 대기실
				&& (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")
						|| !strcmp(msg, "out\n"))) {
			// 채팅 끝남 메시지
			send(hSock, "out", sizeof("out"), 0);
			closesocket(hSock);
			exit(1);
		}

		PER_IO_DATA dataBuf;
		WSAOVERLAPPED overlapped;

		WSAEVENT event = WSACreateEvent();
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		overlapped.hEvent = event;

		P_PACKET_DATA packet;
		packet = new PACKET_DATA;
		if (clientStatus == STATUS_WAITING) { // 대기실 일 때
			if (strcmp(msg, "2") == 0) {
				puts("방 이름을 입력해 주세요");
				gets(msg);
				packet->direction = ROOM_MAKE;
			} else if (strcmp(msg, "3") == 0) {
				puts("입장할 방 이름을 입력해 주세요");
				gets(msg);
				packet->direction = ROOM_ENTER;
			}
		} else if (clientStatus == STATUS_CHATTIG) { // 채팅중일 때

		}
		strcpy(packet->message, msg);

		dataBuf.wsaBuf.buf = (char*) packet;
		dataBuf.wsaBuf.len = sizeof(PACKET_DATA);

		WSASend(hSock, &(dataBuf.wsaBuf), 1, NULL, 0, &overlapped, NULL);
	}
	return 0;
}

// 수신을 담당할 스레드
unsigned WINAPI RecvMsg(LPVOID hComPort) {

	SOCKET sock;
	char nameMsg[BUF_SIZE];
	DWORD bytesTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;

	while (1) {
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &handleInfo,
				(LPOVERLAPPED*) &ioInfo, INFINITE);

		P_PACKET_DATA packet = new PACKET_DATA;
		memcpy(packet, ioInfo->buffer, sizeof(PACKET_DATA));

		// Client의 상태 정보 갱신 필수
		// 서버에서 준것으로 갱신
		if (packet->clientStatus == STATUS_WAITING) {
			clientStatus = packet->clientStatus;
			strcpy(nameMsg, packet->message);
			cout << nameMsg << endl;
		} else if (packet->clientStatus == STATUS_CHATTIG) {
			clientStatus = packet->clientStatus;
			strcpy(nameMsg, packet->message);
			cout << nameMsg << endl;
		}

		sock = handleInfo->hClntSock;
		if (bytesTrans == 0) {
			return -1;
		}

		int recvBytes, flags = 0;
		ioInfo = new PER_IO_DATA;
		ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		WSARecv(sock, &(ioInfo->wsaBuf), 1, (LPDWORD) &recvBytes,
				(LPDWORD) &flags, &(ioInfo->overlapped),
				NULL);

	}
	return 0;
}
int main(int argc, char* argv[0]) {

	WSADATA wsaData;
	SOCKET hSocket;
	SOCKADDR_IN servAddr;
	LPPER_HANDLE_DATA handleInfo;
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

	cout << "이름을 입력해 주세요 :";
	string user;
	cin >> user;
	sprintf(name, "%s", user.c_str());

	// Completion Port 생성
	HANDLE hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	PER_IO_DATA dataBuf;
	WSAOVERLAPPED overlapped;

	WSAEVENT event = WSACreateEvent();
	handleInfo = new PER_HANDLE_DATA;
	handleInfo->hClntSock = hSocket;
	int addrLen = sizeof(servAddr);
	memcpy(&(handleInfo->clntAdr), &servAddr, addrLen);
	memset(&overlapped, 0, sizeof(OVERLAPPED));

	// Completion Port 와 소켓 연결
	CreateIoCompletionPort((HANDLE) hSocket, hComPort, (DWORD) handleInfo, 0);

	overlapped.hEvent = event;
	P_PACKET_DATA packet;
	packet = new PACKET_DATA;
	strcpy(packet->message, name);

	dataBuf.wsaBuf.buf = (char*) packet;
	dataBuf.wsaBuf.len = sizeof(PACKET_DATA);

	// 이름정보 먼저 전달
	WSASend(hSocket, &(dataBuf.wsaBuf), 1, NULL, 0, &overlapped, NULL);

	delete packet;
	// 수신 스레드 동작
	recvThread = (HANDLE) _beginthreadex(NULL, 0, RecvMsg, (LPVOID) hComPort, 0,
	NULL);
	// 송신 스레드 동작
	sendThread = (HANDLE) _beginthreadex(NULL, 0, SendMsg, (void*) &hSocket, 0,
	NULL);

	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	closesocket(hSocket);
	WSACleanup();
	return 0;
}
