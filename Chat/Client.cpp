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

typedef struct { // buffer info
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];
	int serverMode;
	int clientMode;
} PER_IO_DATA, *LPPER_IO_DATA;

unsigned WINAPI SendMsg(void *arg) {
	SOCKET hSock = *((SOCKET*) arg);
	char nameMsg[NAME_SIZE + BUF_SIZE];

	while (1) {
		fgets(msg, BUF_SIZE, stdin);
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
		//overlapped.hEvent = event;

		dataBuf.wsaBuf.buf = nameMsg;
		dataBuf.wsaBuf.len = strlen(nameMsg);
		dataBuf.clientMode = 5;
		WSASend(hSock, &(dataBuf.wsaBuf), 1, NULL, 0, &overlapped, NULL);
	}
	return 0;
}
unsigned WINAPI RecvMsg(void *arg) {
	SOCKET hSock = *((SOCKET*) arg);
	char nameMsg[NAME_SIZE + BUF_SIZE];
	int strLen;

	while (1) {
		strLen = recv(hSock, nameMsg, NAME_SIZE + BUF_SIZE - 1, 0);
		if (strLen == -1) {
			return -1;
		}

		nameMsg[strLen] = 0;
		fputs(nameMsg, stdout);
	}
	return 0;
}
int main(int argc, char* argv[0]) {

	WSADATA wsaData;
	SOCKET hSocket;
	SOCKADDR_IN servAddr;

	HANDLE sendThread, recvThread;

	if (argc != 4) {
		cout << "Usage :" << argv[0] << "<port>" << endl;
		exit(1);
	}

	// Socket lib의 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup() error!");
		exit(1);
	}

	sprintf(name, "%s", argv[3]);
	hSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
	WSA_FLAG_OVERLAPPED);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = PF_INET;
	servAddr.sin_addr.s_addr = inet_addr(argv[1]);
	servAddr.sin_port = htons(atoi(argv[2]));

	if (connect(hSocket, (SOCKADDR*) &servAddr,
			sizeof(servAddr))==SOCKET_ERROR) {
		printf("connect() error!");
		exit(1);
	}
	PER_IO_DATA dataBuf;
	WSAOVERLAPPED overlapped;

	WSAEVENT event = WSACreateEvent();
	memset(&overlapped, 0, sizeof(OVERLAPPED));
	// overlapped.hEvent = event;

	dataBuf.wsaBuf.buf = name;
	dataBuf.wsaBuf.len = BUF_SIZE;
	dataBuf.clientMode = 5;

	WSASend(hSocket, &(dataBuf.wsaBuf), 1, NULL, 0, &overlapped, NULL);

	recvThread = (HANDLE) _beginthreadex(NULL, 0, RecvMsg, (void*) &hSocket, 0,
	NULL);
	sendThread = (HANDLE) _beginthreadex(NULL, 0, SendMsg, (void*) &hSocket, 0,
	NULL);

	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	closesocket(hSocket);
	WSACleanup();
	return 0;
}
