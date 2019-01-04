//============================================================================
// Name        : IocpServer.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
// https://dramadramingdays.tistory.com/119
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>

using namespace std;

#define BUF_SIZE 100
#define START 1
#define READ 2
#define WRITE 3

int const NAME_SIZE = 10;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
	string roomName;
	int status;
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

typedef struct {
	char id[NAME_SIZE];
	unsigned int level;
} USER_INFO, *PUSER_INFO;

map<string, LPPER_HANDLE_DATA> userMap;

HANDLE mutex;

void SendToAllMsg(LPPER_IO_DATA ioInfo, char *msg) {

	printf("SendToAllMsg %s\n", msg);
	ioInfo = (LPPER_IO_DATA) malloc(sizeof(PER_IO_DATA));
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

	ioInfo->wsaBuf.buf = msg;
	ioInfo->wsaBuf.len = BUF_SIZE;
	ioInfo->serverMode = WRITE;
	WaitForSingleObject(mutex, INFINITE);
	map<string, LPPER_HANDLE_DATA>::iterator iter;
	for (iter = userMap.begin(); iter != userMap.end(); iter++) {
		WSASend((*iter->second).hClntSock, &(ioInfo->wsaBuf), 1, NULL, 0,
				&(ioInfo->overlapped), NULL);
	}
	ReleaseMutex(mutex);
}

unsigned WINAPI HandleThread(LPVOID pCompPort) {
	HANDLE hComPort = (HANDLE) pCompPort;
	SOCKET sock;
	DWORD bytesTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;
	DWORD flags = 0;
	char msg[BUF_SIZE];
	char mode[BUF_SIZE];
	P_PACKET_DATA packet;

	while (true) {
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &handleInfo,
				(LPOVERLAPPED*) &ioInfo, INFINITE);
		sock = handleInfo->hClntSock;

		if (START == ioInfo->serverMode) {
			cout << "START" << endl;
			packet = (P_PACKET_DATA) malloc(sizeof(PACKET_DATA));
			memcpy(packet, ioInfo->buffer, sizeof(PACKET_DATA));
			strcpy(msg, packet->name);
			strcpy(mode, packet->message);

			WaitForSingleObject(mutex, INFINITE);

			cout << "START message : " << msg << endl;
			cout << "clientMode : " << mode << endl;

			userMap.insert(pair<string, LPPER_HANDLE_DATA>(msg, handleInfo));
			cout << "현재 접속 인원 수 : " << userMap.size() << endl;

			ReleaseMutex(mutex);

			free(ioInfo);
			strcat(msg, " 님이 입장하셨습니다\n");
			SendToAllMsg(ioInfo, msg);

			ioInfo = (LPPER_IO_DATA) malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->serverMode = READ;

			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags,
					&(ioInfo->overlapped), NULL);

		} else if (READ == ioInfo->serverMode) {
			puts("message received");

			if (bytesTrans == 0 || strcmp(ioInfo->buffer, "out") == 0) {
				WaitForSingleObject(mutex, INFINITE);
				map<string, LPPER_HANDLE_DATA>::iterator iter;
				for (iter = userMap.begin(); iter != userMap.end(); iter++) {
					if (sock == (*iter).second->hClntSock) {
						userMap.erase((*iter).first);
						break;
					}
				}
				cout << "현재 접속 인원 수 : " << userMap.size() << endl;
				ReleaseMutex(mutex);
				closesocket(sock);
				free(handleInfo);
				free(ioInfo);
				continue;
			}

			memset(msg, 0, BUF_SIZE);
			strcpy(msg, ioInfo->buffer);
			free(ioInfo);
			cout << "received message : " << msg << endl;
			SendToAllMsg(ioInfo, msg);
			ioInfo = (LPPER_IO_DATA) malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->serverMode = READ;

			WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags,
					&(ioInfo->overlapped), NULL);
		} else {
			puts("message send");
			//free(ioInfo);
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

	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);

	int process = sysInfo.dwNumberOfProcessors;
	for (i = 0; i < process; i++) {
		_beginthreadex(NULL, 0, HandleThread, (LPVOID) hComPort, 0, NULL);
	}
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

	mutex = CreateMutex(NULL, FALSE, NULL);
	cout << "Server ready listen" << endl;
	cout << "port number" << argv[1] << endl;

	while (true) {
		SOCKET hClientSock;
		SOCKADDR_IN clntAdr;

		int addrLen = sizeof(clntAdr);
		hClientSock = accept(hServSock, (SOCKADDR*) &clntAdr, &addrLen);
		handleInfo = (LPPER_HANDLE_DATA) malloc(sizeof(PER_IO_DATA));
		handleInfo->hClntSock = hClientSock;
		memcpy(&(handleInfo->clntAdr), &clntAdr, addrLen);

		cout << "Connected client IP " << inet_ntoa(clntAdr.sin_addr) << endl;

		CreateIoCompletionPort((HANDLE) hClientSock, hComPort,
				(DWORD) handleInfo, 0);
		ioInfo = (LPPER_IO_DATA) malloc(sizeof(PER_IO_DATA));
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

		ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->serverMode = START;

		WSARecv(handleInfo->hClntSock, &(ioInfo->wsaBuf), 1,
				(LPDWORD) &recvBytes, (LPDWORD) &flags, &(ioInfo->overlapped),
				NULL);
	}

	CloseHandle(mutex);
	return 0;
}
