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

// 현재 클라이언트 상태 => 대기실 => 추후 로그인 이전으로 바뀔것
int clientStatus;

using namespace std;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// 영문+숫자 문자열 판별
bool IsAlphaNumber(char * msg) {
	regex alpha("[a-z|A-Z|0-9]+");
	string str = string(msg);
	if (regex_match(str, alpha)) {
		return true;
	} else {
		return false;
	}
}

// Recv 계속 공통함수
void RecvMore(SOCKET sock, DWORD recvBytes, DWORD flags, LPPER_IO_DATA ioInfo) {

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

// fgets와 \n제거 공통함수 버퍼 오버플로우방지
void Gets(char *message, int size) {
	fgets(message, size, stdin);
	char *p;
	if ((p = strchr(message, '\n')) != NULL) {
		*p = '\0';
	}
}

// WSASend를 call
void SendMsg(SOCKET clientSock, PER_IO_DATA &ioInfo, const char* msg, int status,
		int direction) {

	WSAOVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(OVERLAPPED));

	int len = strlen(msg) + 1;
	char *packet;
	packet = new char[len + (3 * sizeof(int))];
	memcpy(packet, &len, 4); // dataSize;
	memcpy(((char*) packet) + 4, &clientStatus, 4); // status;
	memcpy(((char*) packet) + 8, &direction, 4); // direction;
	memcpy(((char*) packet) + 12, msg, len); // status;

	ioInfo.wsaBuf.buf = (char*) packet;
	ioInfo.wsaBuf.len = len + (3 * sizeof(int));
	ioInfo.serverMode = WRITE;

	WSASend(clientSock, &(ioInfo.wsaBuf), 1, NULL, 0, &overlapped, NULL);
}

// 송신을 담당할 스레드
unsigned WINAPI SendMsgThread(void *arg) {
	// 넘어온 clientSocket을 받아줌
	SOCKET clientSock = *((SOCKET*) arg);
	char msg[BUF_SIZE];

	while (1) {
		Gets(msg, BUF_SIZE);

		PER_IO_DATA dataBuf;

		int direction;
		int status;

		if (clientStatus == STATUS_LOGOUT) { // 로그인 이전
			if (strcmp(msg, "1") == 0) {	// 계정 만들 때

				while (true) {
					cout << "계정을 입력해 주세요 (영문숫자10자리)" << endl;
					Gets(msg, BUF_SIZE);

					if (strcmp(msg, "") != 0 && strlen(msg) <= 10
							&& IsAlphaNumber(msg)) {
						break;
					}
				}

				char password1[NAME_SIZE];
				char password2[NAME_SIZE];

				char nickname[NAME_SIZE];
				while (true) {
					cout << "비밀번호를 입력해 주세요 (영문숫자4자리이상 10자리이하)" << endl;
					Gets(password1, NAME_SIZE);
					if (strcmp(password1, "") == 0) {
						continue;
					} else if (strlen(password1) >= 4 && strlen(password1) <= 10
							&& IsAlphaNumber(password1)) {
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
					cout << "닉네임을 입력해 주세요 (20바이트 이하)" << endl;
					Gets(nickname, NAME_SIZE);

					if (strcmp(nickname, "") != 0 && strlen(nickname) <= 20) {
						break;
					}
				}
				strcat(msg, "\\");
				strcat(msg, password1);  // 비밀번호
				strcat(msg, "\\");
				strcat(msg, nickname); // 닉네임
				direction = USER_MAKE;
				status = STATUS_LOGOUT;

			} else if (strcmp(msg, "2") == 0) { // 로그인 시도
				cout << "계정을 입력해 주세요" << endl;
				Gets(msg, BUF_SIZE);

				char password[NAME_SIZE];

				cout << "비밀번호를 입력해 주세요" << endl;
				Gets(password, NAME_SIZE);

				strcat(msg, "\\");
				strcat(msg, password);  // 비밀번호

				direction = USER_ENTER;
				status = STATUS_LOGOUT;
			} else if (strcmp(msg, "3") == 0) {
				direction = USER_LIST;
				status = STATUS_LOGOUT;
			} else if (strcmp(msg, "4") == 0) { // 클라이언트 강제종료
				exit(1);
			} else if (strcmp(msg, "5") == 0) { // 콘솔지우기
				system("cls");
				cout << loginBeforeMessage << endl;
				continue;
			}
		} else if (clientStatus == STATUS_WAITING) { // 대기실 일 때
			if (strcmp(msg, "1") == 0) {	// 방 만들 때
				direction = ROOM_INFO;
			} else if (strcmp(msg, "2") == 0) {	// 방 만들 때
				cout << "방 이름을 입력해 주세요" << endl;
				Gets(msg, BUF_SIZE);

				direction = ROOM_MAKE;
			} else if (strcmp(msg, "3") == 0) {	// 방 입장할 때
				cout << "입장할 방 이름을 입력해 주세요" << endl;
				Gets(msg, BUF_SIZE);
				direction = ROOM_ENTER;
			} else if (strcmp(msg, "4") == 0) {	// 방 만들 때
				direction = ROOM_USER_INFO;
			} else if (strcmp(msg, "5") == 0) { // 귓속말

				char toName[NAME_SIZE];
				char Msg[BUF_SIZE];
				cout << "귓속말 대상을 입력해 주세요" << endl;
				Gets(toName, NAME_SIZE);
				strncpy(msg, toName, NAME_SIZE);
				cout << "전달할 말을 입력해 주세요" << endl;

				Gets(Msg, BUF_SIZE);
				strcat(msg, "\\");
				strcat(msg, Msg);  // 대상
				direction = WHISPER;
			} else if (strcmp(msg, "7") == 0) { // 콘솔지우기
				system("cls");
				cout << waitRoomMessage << endl;
				continue;
			}
		} else if (clientStatus == STATUS_CHATTIG) { // 채팅중일 때
			if (strcmp(msg, "\\clear") == 0) { // 콘솔창 clear
				system("cls");
				cout << chatRoomMessage << endl;
				continue;
			}
		}

		if (strcmp(msg, "") == 0) { // 입력안하면 Send안함
			continue;
		}

		SendMsg(clientSock, dataBuf, msg, status, direction);
	}
	return 0;
}

// 수신을 담당할 스레드
unsigned WINAPI RecvMsgThread(LPVOID hComPort) {

	SOCKET sock;
	DWORD bytesTrans;
	LPPER_IO_DATA ioInfo;

	while (1) {
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &sock,
				(LPOVERLAPPED*) &ioInfo, INFINITE);

		// IO 완료후 동작 부분
		if (READ == ioInfo->serverMode) {
			memcpy(&(ioInfo->bodySize), ioInfo->buffer, 4);
			ioInfo->recvBuffer = new char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당
			memcpy(((char*) ioInfo->recvBuffer), ioInfo->buffer, bytesTrans);
			ioInfo->recvByte = 0;
			ioInfo->totByte = ioInfo->bodySize + 12;
		} else if (READ_MORE == ioInfo->serverMode) {

			if (ioInfo->recvByte > 12 && ioInfo->recvBuffer != NULL) {
				memcpy(((char*) ioInfo->recvBuffer) + ioInfo->recvByte,
						ioInfo->buffer, bytesTrans);
			}
		}
		if (READ_MORE == ioInfo->serverMode || READ == ioInfo->serverMode) {
			ioInfo->recvByte += bytesTrans;
			if (ioInfo->recvByte < ioInfo->totByte) { // 받은 패킷 부족 -> 더받아야함
				DWORD recvBytes = 0;
				DWORD flags = 0;
				RecvMore(sock, recvBytes, flags, ioInfo); // 패킷 더받기
			} else {
				memcpy(&(ioInfo->serverMode), ((char*) ioInfo->recvBuffer) + 4,
						4);
				char *msg;
				msg = new char[ioInfo->bodySize];

				memcpy(msg, ((char*) ioInfo->recvBuffer) + 12,
						ioInfo->bodySize);

				delete ioInfo->recvBuffer;
				// Client의 상태 정보 갱신 필수
				// 서버에서 준것으로 갱신
				if (ioInfo->serverMode == STATUS_LOGOUT
						|| ioInfo->serverMode == STATUS_WAITING
						|| ioInfo->serverMode == STATUS_CHATTIG) {
					if (clientStatus != ioInfo->serverMode) { // 상태 변경시 콘솔 clear
						system("cls");
					}
					clientStatus = ioInfo->serverMode; // clear이후 client상태변경 해준다
					cout << msg << endl;
				} else if (ioInfo->serverMode == STATUS_WHISPER) { // 귓속말 상태일때는 클라이언트 상태변화 없음
					cout << msg << endl;
				}

				DWORD recvBytes = 0;
				DWORD flags = 0;
				Recv(sock, recvBytes, flags);
			}

			delete ioInfo;
		}

	}
	return 0;
}
int main(int argc, char* argv[0]) {

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

	Recv(hSocket, 0, 0);
	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	closesocket(hSocket);
	WSACleanup();
	return 0;
}
