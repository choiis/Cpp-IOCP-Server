//============================================================================
// Name        : Server.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <unordered_set>
#include "BusinessService.h"
#include "common.h"
#include "CharPool.h"

using namespace std;

// 내부 비지니스 로직 처리 클래스
Service::BusinessService *businessService;

// Recv가 Work에게 전달
queue<JOB_DATA> jobQueue;
// 접속끊어진 socket은 Send에서 제외
unordered_set<SOCKET> liveSocket;

CRITICAL_SECTION liveSocketCs;

CRITICAL_SECTION queueCs;

// DB log insert를 담당하는 스레드
unsigned WINAPI SQLThread(void *arg) {

	while (true) {
		businessService->SQLwork();
	}	
}

// Server 컴퓨터  CPU 개수만큼 스레드 생성될것
// 내부 로직은 각 함수별로 처리
unsigned WINAPI WorkThread(void *arg) {

	while (true) {
		JOB_DATA jobData;
		jobData.job = 0;
		EnterCriticalSection(&queueCs);

		if (jobQueue.size() != 0) {
			jobData = jobQueue.front();
			jobData.job = 1;
			jobQueue.pop();
		}

		LeaveCriticalSection(&queueCs);

		if (jobData.job == 1) {

			EnterCriticalSection(&liveSocketCs);
			unordered_set<SOCKET>::const_iterator itr = liveSocket.find(jobData.socket);
			LeaveCriticalSection(&liveSocketCs);
			if (itr == liveSocket.end()) {
				continue;
			}
			// DataCopy에서 받은 ioInfo 모두 free
			if (!businessService->SessionCheck(jobData.socket)) { // 세션값 없음 => 로그인 이전 분기
				// 로그인 이전 로직 처리
				businessService->StatusLogout(jobData.socket, jobData.direction, jobData.msg.c_str());
				// 세션값 없음 => 로그인 이전 분기 끝
			}
			else { // 세션값 있을때 => 대기방 또는 채팅방 상태

				int status = businessService->BusinessService::GetStatus(
					jobData.socket);

				if (status == STATUS_WAITING) { // 대기실 케이스
					// 대기실 처리 함수
					businessService->StatusWait(jobData.socket, status, jobData.direction,
						jobData.msg.c_str());
				}
				else if (status == STATUS_CHATTIG) { // 채팅 중 케이스
					// 채팅방 처리 함수

					businessService->StatusChat(jobData.socket, status, jobData.direction,
						jobData.msg.c_str());
				}
			}
		}
	}

	return 0;
}

// Server 컴퓨터  CPU 개수만큼 스레드 생성될것
// 내부 로직은 각 함수별로 처리
unsigned WINAPI RecvThread(LPVOID pCompPort) {

	// Completion port객체
	HANDLE hComPort = (HANDLE) pCompPort;
	SOCKET sock;
	short bytesTrans;
	LPPER_IO_DATA ioInfo;

	while (true) {

		bool success = GetQueuedCompletionStatus(hComPort, (LPDWORD)&bytesTrans,
				(LPDWORD) &sock, (LPOVERLAPPED*) &ioInfo, INFINITE);

		if (bytesTrans == 0 && !success) { // 접속 끊김 콘솔 강제 종료
			// 콘솔 강제종료 처리
			EnterCriticalSection(&liveSocketCs);
			liveSocket.erase(sock);
			LeaveCriticalSection(&liveSocketCs);
			businessService->ClientExit(sock);
			MPool* mp = MPool::getInstance();
			mp->Free(ioInfo);
		} else if (READ == ioInfo->serverMode
			|| READ_MORE == ioInfo->serverMode) { // Recv 가 기본 동작
			// 데이터 읽기 과정
			short remainByte = min(bytesTrans, BUF_SIZE); // 초기 Remain Byte
			bool recvMore = false;

			while (1) {
				remainByte = businessService->PacketReading(ioInfo, remainByte);
				// 다 받은 후 정상 로직
				// DataCopy내에서 사용 메모리 전부 반환
				if (remainByte >= 0) {
					JOB_DATA jobData;
					jobData.msg = businessService->DataCopy(ioInfo, &jobData.nowStatus,
						&jobData.direction);
					jobData.socket = sock;
					EnterCriticalSection(&queueCs);
					jobQueue.push(jobData);
					LeaveCriticalSection(&queueCs);
					// JobQueue Insert
				}
				
				if (remainByte == 0) {
					MPool* mp = MPool::getInstance();
					mp->Free(ioInfo);
					break;
				}
				else if (remainByte < 0) { // 받은 패킷 부족 || 헤더 다 못받음 -> 더받아야함
					businessService->BusinessService::getIocpService()->RecvMore(
						sock, ioInfo); // 패킷 더받기 & 기본 ioInfo 보존
					recvMore = true;
					break;
				}
			}
			if (!recvMore) {
				businessService->BusinessService::getIocpService()->Recv(
					sock); // 패킷 더받기
			}
			
		} else if (WRITE == ioInfo->serverMode) { // Send 끝난경우

			CharPool* charPool = CharPool::getInstance();

			charPool->Free(ioInfo->wsaBuf.buf);
			MPool* mp = MPool::getInstance();

			mp->Free(ioInfo);
		} else {

			MPool* mp = MPool::getInstance();
			mp->Free(ioInfo);
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

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup() error!" << endl;
		exit(1);
	}

	// Completion Port 생성
	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);

	int process = sysInfo.dwNumberOfProcessors;
	cout << "Server CPU num : " << process << endl;

	InitializeCriticalSectionAndSpinCount(&queueCs, 2000);

	InitializeCriticalSectionAndSpinCount(&liveSocketCs, 2000);
	
	businessService = new Service::BusinessService();
	
	// Thread Pool Client에게 패킷 받는 동작
	for (int i = 0; i < (process * 3) / 8; i++) {
		// 만들어진 HandleThread를 hComPort CP 오브젝트에 할당한다
		_beginthreadex(NULL, 0, RecvThread, (LPVOID)hComPort, 0, NULL);
	 }

	// Thread Pool Client들에게 보내줄 정보
	for (int i = 0; i < (process * 3) / 2; i++) {
		_beginthreadex(NULL, 0, WorkThread, NULL, 0, NULL);
	}

	// Thread Pool 로그 저장 SQL 실행에 쓰임
	for (int i = 0; i < process / 2; i++) {
		_beginthreadex(NULL, 0, SQLThread, NULL, 0, NULL);
	}
	
	// Overlapped IO가능 소켓을 만든다
	// TCP 통신할것
	hServSock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
	WSA_FLAG_OVERLAPPED);

	string port;
	cout << "포트번호를 입력해 주세요" << endl;
	cin >> port;

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = PF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY뜻은 어느 IP에서 접속이 와도 요청 수락한다는 뜻
	servAdr.sin_port = htons(atoi(port.c_str()));

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

	cout << "Server ready listen" << endl;
	cout << "port number : " << port << endl;

	while (true) {
		SOCKET hClientSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);
		hClientSock = accept(hServSock, (SOCKADDR*) &clntAdr, &addrLen);

		EnterCriticalSection(&liveSocketCs);
		liveSocket.insert(hClientSock);
		LeaveCriticalSection(&liveSocketCs);
		// cout << "Connected client IP " << inet_ntoa(clntAdr.sin_addr) << endl;

		// Completion Port 와 accept한 소켓 연결
		CreateIoCompletionPort((HANDLE) hClientSock, hComPort,
				(DWORD) hClientSock, 0);

		businessService->BusinessService::getIocpService()->Recv(hClientSock);

		// 초기 접속 메세지 Send
		string str = "접속을 환영합니다!\n";
		str += loginBeforeMessage;

		businessService->BusinessService::getIocpService()->SendToOneMsg(
				str.c_str(), hClientSock, STATUS_LOGOUT);
	}
	DeleteCriticalSection(&queueCs);

	DeleteCriticalSection(&liveSocketCs);
	
	delete businessService;

	return 0;
}
