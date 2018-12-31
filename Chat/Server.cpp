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
#include <string.h>
#include <winsock2.h>
#include <process.h>
#include <map>

#define BUF_SIZE 200
using namespace std;

map<string, SOCKET> userMap;

HANDLE hMutex;

void ErrorHandling(char* message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

void SendMsgAll(char* msg, int len) {
	map<string, SOCKET>::iterator iter;

	for (iter = userMap.begin(); iter != userMap.end(); ++iter) {
		send((*iter).second, msg, len, 0);
	}
}

void SendMsg(char* msg, int len) {

	WaitForSingleObject(hMutex, INFINITE);
	SendMsgAll(msg, len);
	ReleaseMutex(hMutex);
}

unsigned WINAPI HandleClnt(void* arg) {
	SOCKET hClntSock = *((SOCKET*) arg);
	int strLen = 0;
	char msg[BUF_SIZE];

	while ((strLen = recv(hClntSock, msg, sizeof(msg), 0)) != 0) {
		cout << msg << endl;
		if (strcmp(msg, "chatting over") == 0) {
			break;
		}
		SendMsg(msg, strLen);
	}

	WaitForSingleObject(hMutex, INFINITE);

	map<string, SOCKET>::iterator iter;

	for (iter = userMap.begin(); iter != userMap.end(); ++iter) {
		if ((*iter).second == hClntSock) {
			cout << (*iter).first << "님 나감" << endl;
			userMap.erase((*iter).first);
			break;
		}
	}
	ReleaseMutex(hMutex);
	closesocket(hClntSock);
	return 0;
}

int main(int argc, char* argv[]) {

	WSADATA wsaData;
	SOCKET hServSock, hClntSock;
	SOCKADDR_IN servAdr, clntAdr;

	int clntAdrSize;

	if (argc != 2) {
		cout << "Usage : " << argv[0] << endl;
		exit(1);
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		ErrorHandling("WSAStartup() error!");
	}

	hMutex = CreateMutex(NULL, FALSE, NULL);
	hServSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = PF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(atoi(argv[1]));

	if (bind(hServSock, (SOCKADDR*) &servAdr, sizeof(servAdr)) == SOCKET_ERROR) {
		ErrorHandling("bind() error!");
	}

	if (listen(hServSock, 5) == SOCKET_ERROR) {
		ErrorHandling("listen() error!");
	}

	cout << "Server ready listen" << endl;

	while (true) {
		clntAdrSize = sizeof(clntAdr);
		hClntSock = accept(hServSock, (SOCKADDR*) &clntAdr, &clntAdrSize);

		WaitForSingleObject(hMutex, INFINITE);
		char intro[BUF_SIZE + 30];
		recv(hClntSock, intro, sizeof(intro), 0);
		userMap.insert(pair<string, SOCKET>(intro, hClntSock));

		cout << "userName : " << intro << endl;
		cout << "Connected client IP " << inet_ntoa(clntAdr.sin_addr) << endl;
		cout << "현재 채팅 인원 : " << userMap.size() << endl;

		strcat(intro, " 님이 입장했습니다");
		cout << intro << endl;
		SendMsgAll(intro, strlen(intro));
		ReleaseMutex(hMutex);

		HANDLE hThread = (HANDLE) _beginthreadex(NULL, 0, HandleClnt,
				(void*) &hClntSock, 0, NULL);
	}

	closesocket(hServSock);
	WSACleanup();

	return EXIT_SUCCESS;
}
