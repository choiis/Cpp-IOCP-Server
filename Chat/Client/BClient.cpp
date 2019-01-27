//============================================================================
// Name        : BClient.cpp
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
#include <queue>
#include <algorithm>
#include <unordered_map>
#include <ctime>
#include <string>
#include "common.h"
#include "MPool.h"
#include "CharPool.h"
#include <list>

//#define _SCL_SECURE_NO_WARNINGS
using namespace std;

CRITICAL_SECTION makeCs;
CRITICAL_SECTION sendCs;
CRITICAL_SECTION userCs;

typedef struct { // socket info
	SOCKET Sock;
	int clientStatus;
	int direction;
	string id;
	string message;
} INFO_CLIENT, *P_INFO_CLIENT;

class ClientQueue {
private:
	unordered_map<SOCKET, INFO_CLIENT> clientMap;
	queue<INFO_CLIENT> makeQueue;
	queue<INFO_CLIENT> sendQueue;
public:
	const queue<INFO_CLIENT>& getMakeQueue() const {
		return makeQueue;
	}

	void setMakeQueue(const queue<INFO_CLIENT> recvQueue) {
		this->makeQueue = recvQueue;
	}

	const queue<INFO_CLIENT>& getSendQueue() const {
		return sendQueue;
	}

	void setSendQueue(const queue<INFO_CLIENT> sendQueue) {
		this->sendQueue = sendQueue;
	}

	// client상태변경 때문에 const 제외
	unordered_map<SOCKET, INFO_CLIENT>& getClientMap() {
		return clientMap;
	}

	void setClientMap(const unordered_map<SOCKET, INFO_CLIENT>& clientMap) {
		this->clientMap = clientMap;
	}
};

ClientQueue clientQueue;

// Recv 계속 공통함수
void RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo) {
	DWORD recvBytes = 0;
	DWORD flags = 0;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = BUF_SIZE;
	memset(ioInfo->buffer, 0, BUF_SIZE);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ_MORE;

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
	LPPER_IO_DATA ioInfo = mp->Malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = BUF_SIZE;
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ;
	ioInfo->recvByte = 0;
	ioInfo->totByte = 0;

	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}

// WSASend를 call
void SendMsg(SOCKET clientSock, const char* msg, int direction) {
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->Malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

	int len = min((int) strlen(msg) + 1, BUF_SIZE - 12); // 최대 보낼수 있는 내용 500Byte
	CharPool* charPool = CharPool::getInstance();
	char* packet = charPool->Malloc(); // 512 Byte까지 읽기 가능

	copy((char*) &len, (char*) &len + 4, packet); // dataSize
	copy((char*) &direction, (char*) &direction + 4, packet + 8);  // direction
	copy(msg, msg + len, packet + 12);  // msg

	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = min(len + 12, BUF_SIZE);
	ioInfo->serverMode = WRITE;

	WSASend(clientSock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped),
	NULL);
}

// 패킷 데이터 읽기
void PacketReading(LPPER_IO_DATA ioInfo, DWORD bytesTrans) {
	// IO 완료후 동작 부분
	if (READ == ioInfo->serverMode) {
		if (bytesTrans < 4) { // 헤더를 다 못 읽어온 상황
			copy(((char*) &(ioInfo->bodySize)) + ioInfo->recvByte,
					((char*) &(ioInfo->bodySize)) + ioInfo->recvByte
							+ bytesTrans, ioInfo->buffer);
			ioInfo->recvByte += bytesTrans; // 지금까지 받은 바이트 수 갱신
		} else {
			copy(ioInfo->buffer, ioInfo->buffer + 4,
					(char*) &(ioInfo->bodySize));
			if (ioInfo->bodySize >= 0 && ioInfo->bodySize <= BUF_SIZE - 12) { // Size 정상
				CharPool* charPool = CharPool::getInstance();
				ioInfo->recvBuffer = charPool->Malloc(); // 512 Byte까지 읽기 가능
				copy(ioInfo->buffer, ioInfo->buffer + bytesTrans,
						((char*) ioInfo->recvBuffer));
				ioInfo->recvByte += bytesTrans; // 지금까지 받은 바이트 수 갱신
			} else { // 잘못된 bodySize; => 더읽기 필요
				ioInfo->recvByte = 0;
				ioInfo->totByte = 0;
				ioInfo->bodySize = 0;
			}
		}
	} else { // 더 읽기
		if (ioInfo->recvByte >= 4) { // 헤더 다 읽었음
			copy(ioInfo->buffer, ioInfo->buffer + bytesTrans,
					((char*) ioInfo->recvBuffer) + ioInfo->recvByte);
			ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
		} else { // 헤더를 다 못읽었음 경우
			int recv = min(4 - ioInfo->recvByte, bytesTrans);
			copy(ioInfo->buffer, ioInfo->buffer + recv,
					((char*) &(ioInfo->bodySize)) + ioInfo->recvByte);
			ioInfo->bodySize = min(ioInfo->bodySize, (DWORD) BUF_SIZE - 12);
			ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
			if (ioInfo->recvByte >= 4) {
				CharPool* charPool = CharPool::getInstance();
				ioInfo->recvBuffer = charPool->Malloc(); // 512 Byte까지 읽기 가능
				copy(((char*) ioInfo->buffer) + recv,
						((char*) ioInfo->buffer) + recv + bytesTrans - recv,
						((char*) ioInfo->recvBuffer) + 4);
			}
		}
	}

	if (ioInfo->totByte == 0 && ioInfo->recvByte >= 4) { // 헤더를 다 읽어야 토탈 바이트 수를 알 수 있다
		ioInfo->totByte = min(ioInfo->bodySize + 12, (DWORD) BUF_SIZE);
	}
}

// 클라이언트에게 받은 데이터 복사후 구조체 해제
char* DataCopy(LPPER_IO_DATA ioInfo, int *status) {
	copy(((char*) ioInfo->recvBuffer) + 4, ((char*) ioInfo->recvBuffer) + 8,
			(char*) status);
	CharPool* charPool = CharPool::getInstance();
	char* msg = charPool->Malloc(); // 512 Byte까지 카피 가능

	copy(((char*) ioInfo->recvBuffer) + 12,
			((char*) ioInfo->recvBuffer) + 12
					+ min(ioInfo->bodySize, (DWORD) BUF_SIZE), msg); //에러위치

	// 다 복사 받았으니 할당 해제
	charPool->Free(ioInfo->recvBuffer);

	MPool* mp = MPool::getInstance();
	mp->Free(ioInfo);

	return msg;
}

// 송신을 담당할 스레드
unsigned WINAPI SendMsgThread(void *arg) {

	while (1) {
		// 보내기 큐를 반환받는다
		EnterCriticalSection(&sendCs);
		queue<INFO_CLIENT> sendQueues = clientQueue.getSendQueue();
		LeaveCriticalSection(&sendCs);
		if (sendQueues.size() != 0) {
			queue<INFO_CLIENT> makeQueues;
			while (!sendQueues.empty()) {
				INFO_CLIENT info = sendQueues.front();
				sendQueues.pop();
				SendMsg(info.Sock, info.message.c_str(),

				info.direction);
				makeQueues.push(info);
			}

			// 다시 넣어줌
			EnterCriticalSection(&sendCs);
			clientQueue.setSendQueue(sendQueues);
			LeaveCriticalSection(&sendCs);

			EnterCriticalSection(&makeCs);
			clientQueue.setMakeQueue(makeQueues);
			LeaveCriticalSection(&makeCs);
		}
	}
	return 0;
}

// 클라이언트의 명령을 만들어 Send에게 보낼 스레드
unsigned WINAPI MakeMsgThread(void *arg) {

	while (1) {
		// 받기 큐를 반환받는다
		EnterCriticalSection(&makeCs);
		queue<INFO_CLIENT> makeQueues = clientQueue.getMakeQueue();
		LeaveCriticalSection(&makeCs);
		string alpha1 = "abcdefghijklmnopqrstuvwxyz";
		string alpha2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		string password = "1234";

		if (makeQueues.size() != 0) {
			queue<INFO_CLIENT> sendQueues;
			while (!makeQueues.empty()) {
				INFO_CLIENT info = makeQueues.front();
				makeQueues.pop();
				EnterCriticalSection(&userCs);
				int cStatus =
						clientQueue.getClientMap().find(info.Sock)->second.clientStatus;
				string idName =
						clientQueue.getClientMap().find(info.Sock)->second.id;
				LeaveCriticalSection(&userCs);
				if (cStatus == STATUS_LOGOUT) {
					int randNum3 = (rand() % 2);
					if (randNum3 % 2 == 1) {
						int randNum1 = (rand() % 2);
						int randNum2 = (rand() % 26);
						string nickName = "";
						if (randNum1 == 0) {
							nickName += alpha1.at(randNum2);
						} else {
							nickName += alpha2.at(randNum2);
						}
						string msg1 = idName;
						msg1.append("\\");
						msg1.append(password); // 비밀번호
						msg1.append("\\");
						msg1.append(idName); // 닉네임
						info.direction = USER_MAKE;

						info.message = msg1;

					} else {
						int randNum1 = (rand() % 2);
						int randNum2 = (rand() % 26);
						string nickName = "";
						if (randNum1 == 0) {
							nickName += alpha1.at(randNum2);
						} else {
							nickName += alpha2.at(randNum2);
						}
						string msg2 = idName;
						msg2.append("\\");
						msg2.append(password); // 비밀번호
						info.direction = USER_ENTER;

						info.message = msg2;
					}
				} else if (cStatus == STATUS_WAITING) {

					int directionNum = (rand() % 10);
					if ((directionNum % 2) == 0) { // 0 2 4 6 8 방입장

						int randNum1 = (rand() % 2);
						int randNum2 = (rand() % 10);
						string roomName = "";
						if (randNum1 == 0) {
							roomName += alpha1.at(randNum2);
						} else {
							roomName += alpha2.at(randNum2);
						}
						info.direction = ROOM_ENTER;

						info.message = roomName;
					} else if ((directionNum % 3) == 0) { // 3 6 9 방만들기
						int randNum1 = (rand() % 2);
						int randNum2 = (rand() % 10);
						string roomName = "";
						if (randNum1 == 0) {
							roomName += alpha1.at(randNum2);
						} else {
							roomName += alpha2.at(randNum2);
						}
						info.direction = ROOM_MAKE;

						info.message = roomName;
					} else if (directionNum == 7) { // 7 방정보
						info.direction = ROOM_INFO;

						info.message = "";
					} else if (directionNum == 1) { // 1 유저정보
						info.direction = ROOM_USER_INFO;

						info.message = "";
					}
				} else if (cStatus == STATUS_CHATTIG) {
					int directionNum = (rand() % 40);
					string msg = "";
					if (directionNum < 39) {
						int randNum1 = (rand() % 2);

						for (int i = 0; i <= (rand() % 60) + 1; i++) {
							int randNum2 = (rand() % 26);
							if (randNum1 == 0) {
								msg += alpha1.at(randNum2);
							} else {
								msg += alpha2.at(randNum2);
							}
						}
					} else { // 40번에 한번 나감
						msg = "\\out";
					}
					info.direction = -1;
					info.message = msg;
				}
				sendQueues.push(info);
			}

			// 다시 넣어줌
			EnterCriticalSection(&makeCs);
			clientQueue.setMakeQueue(makeQueues);
			LeaveCriticalSection(&makeCs);
			EnterCriticalSection(&sendCs);
			clientQueue.setSendQueue(sendQueues);
			LeaveCriticalSection(&sendCs);
		}
	}
	return 0;
}
// 수신을 담당할 스레드
unsigned WINAPI RecvMsgThread(LPVOID hComPort) {

	SOCKET sock;
	DWORD bytesTrans;
	LPPER_IO_DATA ioInfo;

	while (1) {
		bool success = GetQueuedCompletionStatus(hComPort, &bytesTrans,
				(LPDWORD) &sock, (LPOVERLAPPED*) &ioInfo, INFINITE);

		if (bytesTrans == 0 && !success) { // 접속 끊김 콘솔 강제 종료
			// 서버 콘솔 강제종료 처리
			cout << "서버 종료" << endl;
			closesocket(sock);
			MPool* mp = MPool::getInstance();
			mp->Free(ioInfo);
		} else if (READ_MORE == ioInfo->serverMode
				|| READ == ioInfo->serverMode) {
			// 데이터 읽기 과정
			PacketReading(ioInfo, bytesTrans);

			if (((ioInfo->recvByte < ioInfo->totByte)
					|| (ioInfo->recvByte < 4 && ioInfo->totByte == 0))
					&& ioInfo->recvByte <= BUF_SIZE) { // 받은 패킷 부족 || 헤더 다 못받음 -> 더받아야함

				RecvMore(sock, ioInfo); // 패킷 더받기
			} else if (ioInfo->recvByte <= BUF_SIZE) {
				int status;
				char *msg = DataCopy(ioInfo, &status);

				// Client의 상태 정보 갱신 필수
				// 서버에서 준것으로 갱신
				if (status == STATUS_LOGOUT || status == STATUS_WAITING
						|| status == STATUS_CHATTIG) {
					EnterCriticalSection(&userCs);

					unordered_map<SOCKET, INFO_CLIENT> clientMap =
							clientQueue.getClientMap();
					clientMap.find(sock)->second.clientStatus = status; // clear이후 client상태변경 해준다
					clientQueue.setClientMap(clientMap);

					LeaveCriticalSection(&userCs);

				}
				CharPool* charPool = CharPool::getInstance();
				charPool->Free(msg);
				Recv(sock);
			} else {
				// 다 복사 받았으니 할당 해제
				CharPool* charPool = CharPool::getInstance();
				charPool->Free(ioInfo->recvBuffer);

				MPool* mp = MPool::getInstance();
				mp->Free(ioInfo);
				Recv(sock);
			}
		} else if (WRITE == ioInfo->serverMode) {
			MPool* mp = MPool::getInstance();
			CharPool* charPool = CharPool::getInstance();
			charPool->Free(ioInfo->wsaBuf.buf);
			mp->Free(ioInfo);
		}
	}
	return 0;
}

int main() {

	WSADATA wsaData;

	// Socket lib의 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup() error!");
		exit(1);
	}

	cout << "포트번호를 입력해 주세요 :";
	string port;
	cin >> port;

	DWORD clientCnt;
	cout << "클라이언트 수를 입력해 주세요";
	cin >> clientCnt;
	queue<INFO_CLIENT> recvQueues;
	queue<INFO_CLIENT> sendQueues;
	unordered_map<SOCKET, INFO_CLIENT> userMap;
	InitializeCriticalSectionAndSpinCount(&makeCs, 2000);

	InitializeCriticalSectionAndSpinCount(&sendCs, 2000);

	InitializeCriticalSectionAndSpinCount(&userCs, 2000);

	// Completion Port 생성
	HANDLE hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	srand((unsigned int) time(NULL));

	for (DWORD i = 0; i < clientCnt; i++) {
		SOCKET clientSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
		NULL, 0,
		WSA_FLAG_OVERLAPPED);
		SOCKADDR_IN servAddr;
		memset(&servAddr, 0, sizeof(servAddr));
		servAddr.sin_family = PF_INET;
		servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		servAddr.sin_port = htons(atoi(port.c_str()));

		if (connect(clientSocket, (SOCKADDR*) &servAddr,
				sizeof(servAddr)) == SOCKET_ERROR) {
			printf("connect() error!");
		}

		// Completion Port 와 소켓 연결
		CreateIoCompletionPort((HANDLE) clientSocket, hComPort,
				(DWORD) clientSocket, 0);
		// 초기화한 상태를 queue에 insert
		INFO_CLIENT info;
		info.Sock = clientSocket;
		info.clientStatus = STATUS_LOGOUT;
		string alpha1 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		info.id = "";
		int j = i % 52;
		for (DWORD k = 0; k <= i / 52; k++) {
			info.id += alpha1.at(j);
		}

		// Recv부터 간다
		recvQueues.push(info);
		// userMap 채운다
		userMap[clientSocket] = info;
		cout << "sock num " << clientSocket << " id " << info.id << endl;
		// 각 socket들 Recv 동작으로
		Recv(clientSocket);

	}
	// 전체 clientUser
	clientQueue.setClientMap(userMap);
	// Recv큐는 채워서
	clientQueue.setMakeQueue(recvQueues);
	// Send큐는 빈상태로
	clientQueue.setSendQueue(sendQueues);

	cout << "현재 접속 클라이언트 수" << clientCnt << endl;

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	int process = sysInfo.dwNumberOfProcessors;

	// 수신 스레드 동작
	// 만들어진 RecvMsg를 hComPort CP 오브젝트에 할당한다
	// RecvMsg에서 Recv가 완료되면 동작할 부분이 있다
	for (int i = 0; i < process; i++) {
		_beginthreadex(NULL, 0, RecvMsgThread, (LPVOID) hComPort, 0,
		NULL);
	}

	// 동작을 만들어줄 스레드 동작
	// 만들어진 동작큐를 hComPort SendMsgThread에서 처리하게 된다
	for (int i = 0; i < process; i++) {
		_beginthreadex(NULL, 0, MakeMsgThread,
		NULL, 0,
		NULL);
	}

	// 송신 스레드 동작
	// Thread안에서 clientSocket으로 Send해줄거니까 인자로 넘겨준다
	// CP랑 Send는 연결 안되어있음 GetQueuedCompletionStatus에서 Send 완료처리 필요없음
	for (int i = 0; i < process; i++) {
		_beginthreadex(NULL, 0, SendMsgThread,
		NULL, 0,
		NULL);
	}

	while (true) {
		int moreClient;
		int speed;

		//	cout << " 더만들 클라이언트 수를 입력하세요" << endl;
		cin >> moreClient;

		//	cout << " 패킷 보낼 1/1000 초 단위를 입력하세요" << endl;
		//	cin >> speed;

	}
	// 임계영역 Object 반환
	DeleteCriticalSection(&makeCs);
	DeleteCriticalSection(&sendCs);
	DeleteCriticalSection(&userCs);

	unordered_map<SOCKET, INFO_CLIENT>::iterator iter;

	for (iter = clientQueue.getClientMap().begin();
			iter != clientQueue.getClientMap().end(); ++iter) {
		closesocket(iter->first);
	}
	WSACleanup();
	return 0;
}
