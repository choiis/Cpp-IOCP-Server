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

#define BUF_SIZE 100
#define NAME_SIZE 20

char name[NAME_SIZE] = "";
char msg[BUF_SIZE];

using namespace std;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct {
	int status;
	char name[BUF_SIZE];
	char message[BUF_SIZE];
} PACKET_DATA, *P_PACKET_DATA;

typedef struct { // buffer info
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[sizeof(PACKET_DATA)];
	int serverMode;
	int clientMode;
} PER_IO_DATA, *LPPER_IO_DATA;

unsigned WINAPI SendMsg(void *arg) {
	SOCKET hSock = *((SOCKET*) arg);
	char nameMsg[BUF_SIZE];

	while (1) {
		cin >> msg;
		//fgets(msg, BUF_SIZE, stdin);

		if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")
				|| !strcmp(msg, "out\n")) {
			// 채팅 끝남 메시지
			send(hSock, "out", sizeof("out"), 0);
			closesocket(hSock);
			exit(1);
		}
		sprintf(nameMsg, "%s : %s", name, msg);
		PER_IO_DATA dataBuf;
		WSAOVERLAPPED overlapped;

		WSAEVENT event = WSACreateEvent();
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		overlapped.hEvent = event;

		dataBuf.wsaBuf.buf = nameMsg;
		dataBuf.wsaBuf.len = BUF_SIZE;
		dataBuf.clientMode = 5;
		WSASend(hSock, &(dataBuf.wsaBuf), 1, NULL, 0, &overlapped, NULL);
	}
	return 0;
}
unsigned WINAPI RecvMsg(LPVOID hComPort) {

	SOCKET sock;
	char nameMsg[BUF_SIZE];
	DWORD bytesTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;

	while (1) {
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &handleInfo,
				(LPOVERLAPPED*) &ioInfo, INFINITE);
		cout << "recv Ok" << endl;
		strcpy(nameMsg, ioInfo->buffer);
		fputs(nameMsg, stdout);

		sock = handleInfo->hClntSock;
		if (bytesTrans == 0) {
			return -1;
		}

		int recvBytes, flags = 0;
		ioInfo = (LPPER_IO_DATA) malloc(sizeof(PER_IO_DATA));
		ioInfo->wsaBuf.len = BUF_SIZE;
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

//	if (argc != 4) {
//		cout << "Usage :" << argv[0] << "<port>" << endl;
//		exit(1);
//	}

	// Socket lib의 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup() error!");
		exit(1);
	}

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
			exit(1);
		} else {
			break;
		}
	}

	cout << "이름을 입력해 주세요 :";
	string user;
	cin >> user;
	sprintf(name, "%s", user.c_str());
	HANDLE hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	PER_IO_DATA dataBuf;
	WSAOVERLAPPED overlapped;

	WSAEVENT event = WSACreateEvent();
	handleInfo = (LPPER_HANDLE_DATA) malloc(sizeof(PER_IO_DATA));
	handleInfo->hClntSock = hSocket;
	int addrLen = sizeof(servAddr);
	memcpy(&(handleInfo->clntAdr), &servAddr, addrLen);
	memset(&overlapped, 0, sizeof(OVERLAPPED));

	CreateIoCompletionPort((HANDLE) hSocket, hComPort, (DWORD) handleInfo, 0);

	overlapped.hEvent = event;
	P_PACKET_DATA packet;
	packet = (P_PACKET_DATA) malloc(sizeof(PACKET_DATA));
	strcpy(packet->name, name);
	strcpy(packet->message, "최초 접속 모드");

	dataBuf.wsaBuf.buf = (char*) packet;
	dataBuf.wsaBuf.len = sizeof(PACKET_DATA);

	WSASend(hSocket, &(dataBuf.wsaBuf), 1, NULL, 0, &overlapped, NULL);

	free(packet);
	recvThread = (HANDLE) _beginthreadex(NULL, 0, RecvMsg, (LPVOID) hComPort, 0,
	NULL);
	sendThread = (HANDLE) _beginthreadex(NULL, 0, SendMsg, (void*) &hSocket, 0,
	NULL);

	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	closesocket(hSocket);
	WSACleanup();
	return 0;
}
