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
#include "common.h"

// 현재 클라이언트 상태 => 대기실 => 추후 로그인 이전으로 바뀔것
int clientStatus;

using namespace std;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// fgets와 \n제거 공통함수 버퍼 오버플로우방지
void Gets(char *message, int size) {
	fgets(message, size, stdin);
	char *p;
	if ((p = strchr(message, '\n')) != NULL) {
		*p = '\0';
	}
}

// 송신을 담당할 스레드
unsigned WINAPI SendMsgThread(void *arg) {
	// 넘어온 clientSocket을 받아줌
	SOCKET clientSock = *((SOCKET*) arg);
	char msg[BUF_SIZE];

	while (1) {
		Gets(msg, BUF_SIZE);

		PER_IO_DATA dataBuf;
		WSAOVERLAPPED overlapped;

		memset(&overlapped, 0, sizeof(OVERLAPPED));

		P_PACKET_DATA packet;
		packet = new PACKET_DATA;
		if (clientStatus == STATUS_LOGOUT) { // 로그인 이전
			if (strcmp(msg, "1") == 0) {	// 계정 만들 때

				while (true) {
					cout << "계정을 입력해 주세요" << endl;
					Gets(msg, BUF_SIZE);

					if (strcmp(msg, "") != 0) {
						break;
					}
				}

				char password1[NAME_SIZE];
				char password2[NAME_SIZE];

				char nickname[NAME_SIZE];
				while (true) {
					cout << "비밀번호를 입력해 주세요" << endl;
					Gets(password1, NAME_SIZE);
					if (strcmp(password1, "") == 0) {
						continue;
					} else {
						break;
					}
				}

				while (true) {
					cout << "비밀번호를 한번더 입력해 주세요" << endl;
					Gets(password2, NAME_SIZE);

					if (strcmp(password2, "") == 0) {
						continue;
					}

					if (strcmp(password1, password2) != 0) { // 비밀번호 다름
						cout << "비밀번호 확인 실패!" << endl;
					} else {
						break;
					}
				}

				while (true) {
					cout << "닉네임을 입력해 주세요" << endl;
					Gets(nickname, NAME_SIZE);

					if (strcmp(nickname, "") != 0) {
						break;
					}
				}

				strncpy(packet->message2, password1, NAME_SIZE); // 비밀번호
				strncpy(packet->message3, nickname, NAME_SIZE); // 닉네임
				packet->direction = USER_MAKE;
				packet->clientStatus = STATUS_LOGOUT;

			} else if (strcmp(msg, "2") == 0) { // 로그인 시도
				cout << "계정을 입력해 주세요" << endl;
				Gets(msg, BUF_SIZE);

				char password[NAME_SIZE];

				cout << "비밀번호를 입력해 주세요" << endl;
				Gets(password, NAME_SIZE);

				strncpy(packet->message2, password, NAME_SIZE);

				packet->direction = USER_ENTER;
				packet->clientStatus = STATUS_LOGOUT;
			} else if (strcmp(msg, "3") == 0) {
				packet->direction = USER_LIST;
				packet->clientStatus = STATUS_LOGOUT;
			} else if (strcmp(msg, "4") == 0) { // 클라이언트 강제종료
				exit(1);
			} else if (strcmp(msg, "5") == 0) { // 콘솔지우기
				system("cls");
				cout << loginBeforeMessage << endl;
				continue;
			} else if (strcmp(msg, "") == 0) { // 입력안하면 Send안함
				continue;
			}

		} else if (clientStatus == STATUS_WAITING) { // 대기실 일 때
			if (strcmp(msg, "2") == 0) {	// 방 만들 때
				cout << "방 이름을 입력해 주세요" << endl;
				Gets(msg, BUF_SIZE);

				packet->direction = ROOM_MAKE;
			} else if (strcmp(msg, "3") == 0) {	// 방 입장할 때
				cout << "입장할 방 이름을 입력해 주세요" << endl;
				Gets(msg, BUF_SIZE);
				packet->direction = ROOM_ENTER;
			} else if (strcmp(msg, "5") == 0) { // 귓속말

				char toName[NAME_SIZE];
				cout << "귓속말 대상을 입력해 주세요" << endl;
				Gets(toName, NAME_SIZE);

				cout << "전달할 말을 입력해 주세요" << endl;
				Gets(msg, BUF_SIZE);
				strncpy(packet->message2, toName, NAME_SIZE);
				packet->direction = WHISPER;
			} else if (strcmp(msg, "7") == 0) { // 콘솔지우기
				system("cls");
				cout << waitRoomMessage << endl;
				continue;
			} else if (strcmp(msg, "") == 0) { // 입력안하면 Send안함
				continue;
			}
		} else if (clientStatus == STATUS_CHATTIG) { // 채팅중일 때
			if (strcmp(msg, "") == 0) { // 입력안하면 Send안함
				continue;
			} else if (strcmp(msg, "clear") == 0) { // 콘솔창 clear
				system("cls");
				cout << chatRoomMessage << endl;
				continue;
			}
		}
		strncpy(packet->message, msg, BUF_SIZE);
		packet->clientStatus = clientStatus;
		dataBuf.wsaBuf.buf = (char*) packet;
		dataBuf.wsaBuf.len = sizeof(PACKET_DATA);
		dataBuf.serverMode = WRITE;

		WSASend(clientSock, &(dataBuf.wsaBuf), 1, NULL, 0, &overlapped, NULL);
	}
	return 0;
}

// 수신을 담당할 스레드
unsigned WINAPI RecvMsgThread(LPVOID hComPort) {

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
		if (packet->clientStatus == STATUS_LOGOUT
				|| packet->clientStatus == STATUS_WAITING
				|| packet->clientStatus == STATUS_CHATTIG) {
			if (clientStatus != packet->clientStatus) { // 상태 변경시 콘솔 clear
				system("cls");
			}
			clientStatus = packet->clientStatus; // clear이후 client상태변경 해준다
			strncpy(nameMsg, packet->message, BUF_SIZE);
			cout << nameMsg << endl;
		} else if (packet->clientStatus == STATUS_WHISPER) { // 귓속말 상태일때는 클라이언트 상태변화 없음
			strncpy(nameMsg, packet->message, BUF_SIZE);
			cout << nameMsg << endl;
		}

		sock = handleInfo->hClntSock;
		if (bytesTrans == 0) {
			return -1;
		}

		DWORD recvBytes = 0;
		DWORD flags = 0;
		ioInfo = new PER_IO_DATA;
		ioInfo->wsaBuf.len = sizeof(PACKET_DATA);
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->serverMode = READ;

		WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
				&(ioInfo->overlapped),
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
	// Completion Port 생성
	HANDLE hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	handleInfo = new PER_HANDLE_DATA;
	handleInfo->hClntSock = hSocket;
	int addrLen = sizeof(servAddr);
	memcpy(&(handleInfo->clntAdr), &servAddr, addrLen);

	// Completion Port 와 소켓 연결
	CreateIoCompletionPort((HANDLE) hSocket, hComPort, (DWORD) handleInfo, 0);

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

	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	closesocket(hSocket);
	WSACleanup();
	return 0;
}
