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

#include "BusinessService.h"

using namespace std;

// 내부 비지니스 로직 처리 클래스
Service::BusinessService *handle;

// Server 컴퓨터  CPU 개수만큼 스레드 생성될것
// 내부 로직은 각 함수별로 처리
unsigned WINAPI HandleThread(LPVOID pCompPort) {

	// Completion port객체
	HANDLE hComPort = (HANDLE) pCompPort;
	SOCKET sock;
	DWORD bytesTrans;
	LPPER_IO_DATA ioInfo;

	while (true) {

		bool success = GetQueuedCompletionStatus(hComPort, &bytesTrans,
				(LPDWORD) &sock, (LPOVERLAPPED*) &ioInfo, INFINITE);

		if (bytesTrans == 0 && !success) { // 접속 끊김 콘솔 강제 종료
			// 콘솔 강제종료 처리
			handle->ClientExit(sock);

			// delete ioInfo; // 전달info 해제
			handle->free(ioInfo);
		} else if (READ == ioInfo->serverMode
				|| READ_MORE == ioInfo->serverMode) { // Recv 끝난경우

				// 데이터 읽기 과정
			handle->PacketReading(ioInfo, bytesTrans);
			if ((ioInfo->recvByte < ioInfo->totByte)
					|| (ioInfo->recvByte < 4 && ioInfo->totByte == 0)) { // 받은 패킷 부족 || 헤더 다 못받음 -> 더받아야함

				LPPER_IO_DATA ioInfo = handle->malloc();
				handle->BusinessService::getIocpService()->RecvMore(sock,
						ioInfo); // 패킷 더받기
			} else { // 다 받은 후 정상 로직
				int clientStatus;
				int direction;
				char *msg = handle->DataCopy(ioInfo, &clientStatus, &direction);

				if (handle->getUserMap().find(sock)
						== handle->getUserMap().end()) { // 세션값 없음 => 로그인 이전 분기
						// 로그인 이전 로직 처리
					handle->StatusLogout(sock, clientStatus, direction, msg);
					// Recv는 계속한다
					LPPER_IO_DATA ioInfo = handle->malloc();
					handle->BusinessService::getIocpService()->Recv(sock,
							ioInfo); // 패킷 더받기
					// 세션값 없음 => 로그인 이전 분기 끝
				} else { // 세션값 있을때 => 대기방 또는 채팅방 상태

					int status = handle->getUserMap().find(sock)->second->status;

					if (status == STATUS_WAITING) { // 대기실 케이스
						// 대기실 처리 함수
						handle->StatusWait(sock, status, direction, msg);
					} else if (status == STATUS_CHATTIG) { // 채팅 중 케이스
						// 채팅방 처리 함수
						handle->StatusChat(sock, status, direction, msg);
					}

					// Recv는 계속한다
					LPPER_IO_DATA ioInfo = handle->malloc();
					handle->BusinessService::getIocpService()->Recv(sock,
							ioInfo); // 패킷 더받기
				}

			}
		} else if (WRITE == ioInfo->serverMode) { // Send 끝난경우
			cout << "message send :" << ioInfo->recvByte << endl;
			ioInfo->recvByte++; // 보냄완료 카운트 +1
			if (ioInfo->totByte == ioInfo->recvByte) { // 보두에게 다 보낸후 해제
				delete ioInfo->wsaBuf.buf; // packet위치를 해제
				// delete ioInfo; // 전달info 해제
				handle->free(ioInfo);
			}
		}
	}
	return 0;
}

int main(int argc, char* argv[]) {

	WSADATA wsaData;
	HANDLE hComPort;
	SYSTEM_INFO sysInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAdr;

	if (argc != 2) {
		cout << "Usage : " << argv[0] << endl;
		exit(1);
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup() error!" << endl;
		exit(1);
	}

	// Completion Port 생성
	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);

	int process = sysInfo.dwNumberOfProcessors;
	cout << "Server CPU num : " << process << endl;

	// CPU 갯수 만큼 스레드 생성
	// Thread Pool 스레드를 필요한 만큼 만들어 놓고 파괴 안하고 사용
	for (int i = 0; i < process; i++) {
		// 만들어진 HandleThread를 hComPort CP 오브젝트에 할당한다
		_beginthreadex(NULL, 0, HandleThread, (LPVOID) hComPort, 0, NULL);
		// 첫번째는 security관련 NULL 쓰며됨
		// 다섯번째 0은 스레드 곧바로 시작 의미
		// 여섯번째는 스레드 아이디
	}

	// Overlapped IO가능 소켓을 만든다
	// TCP 통신할것
	hServSock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
	WSA_FLAG_OVERLAPPED);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = PF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY뜻은 어느 IP에서 접속이 와도 요청 수락한다는 뜻
	servAdr.sin_port = htons(atoi(argv[1]));

	if (bind(hServSock, (SOCKADDR*) &servAdr, sizeof(servAdr)) == SOCKET_ERROR) {
		cout << "bind() error!" << endl;
		exit(1);
	}

	if (listen(hServSock, 5) == SOCKET_ERROR) {
		// listen의 두번째 인자는 접속 대기 큐의 크기
		// accept작업 다 끝나기전에 대기 할 공간
		cout << "listen() error!" << endl;
		exit(1);
	}

	handle = new Service::BusinessService();

	cout << "Server ready listen" << endl;
	cout << "port number : " << argv[1] << endl;

	while (true) {
		SOCKET hClientSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);
		cout << "accept wait" << endl;
		hClientSock = accept(hServSock, (SOCKADDR*) &clntAdr, &addrLen);

		cout << "Connected client IP " << inet_ntoa(clntAdr.sin_addr) << endl;

		// Completion Port 와 accept한 소켓 연결
		CreateIoCompletionPort((HANDLE) hClientSock, hComPort,
				(DWORD) hClientSock, 0);
		LPPER_IO_DATA ioInfo1 = handle->malloc();
		handle->BusinessService::getIocpService()->Recv(hClientSock, ioInfo1);

		// 초기 접속 메세지 Send
		string str = "접속을 환영합니다!\n";
		str += loginBeforeMessage;
		LPPER_IO_DATA ioInfo2 = handle->malloc();
		handle->BusinessService::getIocpService()->SendToOneMsg(str.c_str(),
				hClientSock, STATUS_LOGOUT, ioInfo2);
	}

	delete handle;

	return 0;
}
