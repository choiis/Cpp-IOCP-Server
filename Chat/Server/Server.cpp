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
#include <direct.h>
#include <atomic>
#include "Socket.h"
#include "BusinessService.h"

using namespace std;

// 내부 비지니스 로직 처리 클래스
BusinessService::BusinessService *businessService;

// Recv가 Work에게 전달
queue<JOB_DATA> jobQueue;

mutex queueCs;

atomic<int> packetCnt = 0;

// DB log insert를 담당하는 스레드
unsigned WINAPI SQLThread(void *arg) {

	while (true) {
		businessService->SQLwork();
	}

	return 0;
}

// Send broadcast 담당하는 스레드
unsigned WINAPI SendThread(void *arg) {

	while (true) {
		businessService->Sendwork();
	}
	return 0;
}


// Server 컴퓨터  CPU 개수만큼 스레드 생성될것
// 내부 로직은 각 함수별로 처리
unsigned WINAPI WorkThread(void *arg) {

	while (true) {

		if (!jobQueue.empty()) {

			queue<JOB_DATA> copyJobQueue;
			{
				lock_guard<mutex> guard(queueCs);  // Lock최소화
				// 큐 통채 복사
				copyJobQueue = jobQueue;
				queue<JOB_DATA> emptyQueue; // 빈 큐
				swap(jobQueue, emptyQueue); // 빈 큐로 바꿔치기
			}
			
			while (!copyJobQueue.empty()) { // 여러 패킷 데이터 한꺼번에 처리
				JOB_DATA jobData = copyJobQueue.front();
				copyJobQueue.pop();

				if (businessService->IsSocketDead(jobData.socket)) {
					continue;
				}


				// DataCopy에서 받은 ioInfo 모두 free
				if (!businessService->SessionCheck(jobData.socket)) { // 세션값 없음 => 로그인 이전 분기
					// 로그인 이전 로직 처리

					if (jobData.direction == Direction::CALLCOUNT) { // node js 서버로 전체 로그인된 유저수 

						int cnt = packetCnt;
						packetCnt = 0;
						businessService->CallCnt(jobData.socket, cnt);
					}
					else if (jobData.direction == Direction::BAN) {
						businessService->BanUser(jobData.socket, jobData.msg.substr(0, jobData.nowStatus).c_str());
					}
					else if (jobData.direction == Direction::EXIT) {
						// 정상종료
						exit(EXIT_SUCCESS);
					}
					else { // 관리콘솔이 아니라 클라이언트에서 보낼 때
						businessService->StatusLogout(jobData.socket, jobData.direction, jobData.msg.c_str());
					}
					// 세션값 없음 => 로그인 이전 분기 끝
				}
				else { // 세션값 있을때 => 대기방 또는 채팅방 상태

					int status = businessService->GetStatus(
						jobData.socket);

					if (status == ClientStatus::STATUS_WAITING && jobData.direction != -1) { // 대기실 케이스
						// 대기실 처리 함수
						businessService->StatusWait(jobData.socket, status, jobData.direction,
							jobData.msg.c_str());
					}
					else if (status == ClientStatus::STATUS_CHATTIG) { // 채팅 중 케이스
						// 채팅방 처리 함수

						if (jobData.direction == Direction::FILE_SEND) { // 파일 전송 케이스
							businessService->StatusFile(jobData.socket);
						}
						else { // 일반 채팅일때
							businessService->StatusChat(jobData.socket, status, jobData.direction,
								jobData.msg.c_str());
						}

					}
				}
			}
		}
		else {  // 동작안할  다른 스레드가 일할 수 있게
			Sleep(1);
		}

	}

	return 0;
}

// Server 컴퓨터  CPU 개수만큼 스레드 생성될것
// 내부 로직은 각 함수별로 처리
unsigned WINAPI RecvThread(LPVOID pCompPort) {

	// Completion port객체
	HANDLE hComPort = (HANDLE)pCompPort;
	SOCKET sock;
	short bytesTrans;
	LPPER_IO_DATA ioInfo;

	while (true) {

		bool success = GetQueuedCompletionStatus(hComPort, (LPDWORD)&bytesTrans,
			(LPDWORD)&sock, (LPOVERLAPPED*)&ioInfo, INFINITE);

		if (bytesTrans == 0 && !success) { // 접속 끊김 콘솔 강제 종료

			int errorNum = WSAGetLastError();
			switch (errorNum)
			{
			case ERROR_IO_PENDING: // 대기열이 혼잡한 경우
				cout << "ERROR_IO_PENDING " << endl;
				break;
			case ERROR_NETNAME_DELETED: // ungraceful close
				cout << "ERROR_NETNAME_DELETED " << endl;
				break;
			case ERROR_SEM_TIMEOUT: // 네트워크단절현상으로 timeout
				cout << "ERROR_SEM_TIMEOUT " << endl;
				break;
			case ERROR_OPERATION_ABORTED: // socket close Event
				cout << "ERROR_OPERATION_ABORTED " << endl;
				break;
			default:
				cout << "else " << endl;
				break;
			}

			businessService->ClientExit(sock);
			MPool* mp = MPool::getInstance();
			mp->Free(ioInfo);
		}
		else if (businessService->getIocpService()->RECV == ioInfo->serverMode
			|| businessService->getIocpService()->RECV_MORE == ioInfo->serverMode) { // Recv 가 기본 동작
			// 데이터 읽기 과정
			short remainByte = min(bytesTrans, BUF_SIZE); // 초기 Remain Byte
			bool recvMore = false;

			// jobQueue에 데이터를 한번에 담기위한 자료구조
			queue<JOB_DATA> packetQueue;

			while (true) {
				remainByte = businessService->PacketReading(ioInfo, remainByte);
				// 다 받은 후 정상 로직
				// DataCopy내에서 사용 메모리 전부 반환
				if (remainByte >= 0) {
					JOB_DATA jobData;
					jobData.msg = businessService->DataCopy(ioInfo, &jobData.nowStatus,
						&jobData.direction);
					jobData.socket = sock;
					packetQueue.push(jobData);
					// packetQueue 채움
				}

				if (remainByte == 0) {
					MPool* mp = MPool::getInstance();
					mp->Free(ioInfo);
					break;
				}
				else if (remainByte < 0) { // 받은 패킷 부족 || 헤더 다 못받음 -> 더받아야함
					businessService->getIocpService()->RecvMore(
						sock, ioInfo); // 패킷 더받기 & 기본 ioInfo 보존

					recvMore = true;
					break;
				}
			}

			{
				lock_guard<mutex> guard(queueCs);
				packetCnt.fetch_add(packetQueue.size());
				while (!packetQueue.empty()) { // packetQueue -> jobQueue 
					JOB_DATA jobData = packetQueue.front();
					packetQueue.pop();
					jobQueue.push(jobData);
				}
			}
			// jobQueue에 한번에 Insert

			if (!recvMore) { // recvMore이 아니면 해당 socket은 받기 동작을 계속한다
				businessService->getIocpService()->Recv(
					sock); // 패킷 더받기
			}
		}
		else if (businessService->getIocpService()->SEND == ioInfo->serverMode) { // Send 끝난경우

			CharPool* charPool = CharPool::getInstance();

			charPool->Free(ioInfo->wsaBuf.buf);
			MPool* mp = MPool::getInstance();

			mp->Free(ioInfo);
		}
		else {

			MPool* mp = MPool::getInstance();
			mp->Free(ioInfo);
		}
	}
	return 0;
}


int main(int argc, char* argv[]) {

	HANDLE hComPort;
	SYSTEM_INFO sysInfo;

	Socket socket;
	
	SOCKET hServSock = socket.getSocket();

	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE), 10);

	// Completion Port 생성
	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);

	cout << "Server ready listen" << endl;
	cout << "port number : " << SERVER_PORT << endl;

	int process = sysInfo.dwNumberOfProcessors;
	cout << "Server CPU num : " << process << endl;

	MPool* mp = MPool::getInstance(); // 메모리풀 초기화 지점
	CharPool* charPool = CharPool::getInstance(); // 메모리풀 초기화 지점

	businessService = new BusinessService::BusinessService();

	// Thread Pool Client에게 패킷 받는 동작
	for (int i = 0; i < process; i++) {
		// 만들어진 HandleThread를 hComPort CP 오브젝트에 할당한다
		_beginthreadex(NULL, 0, RecvThread, (LPVOID)hComPort, 0, NULL);
	}

	// Thread Pool 비지니스 로직 담당
	for (int i = 0; i < 2 * process; i++) {
		_beginthreadex(NULL, 0, WorkThread, NULL, 0, NULL);
	}

	// Thread Pool 로그 저장 SQL 실행에 쓰임
	for (int i = 0; i < process; i++) {
		_beginthreadex(NULL, 0, SQLThread, NULL, 0, NULL);
	}

	// Thread Pool BroadCast 해줌
	for (int i = 0; i < (process * 5) / 3; i++) {
		_beginthreadex(NULL, 0, SendThread, NULL, 0, NULL);
	}

	while (true) {
		SOCKET hClientSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);
		hClientSock = accept(hServSock, (SOCKADDR*)&clntAdr, &addrLen);

		businessService->InsertLiveSocket(hClientSock, clntAdr);

		// Completion Port 와 accept한 소켓 연결
		CreateIoCompletionPort((HANDLE)hClientSock, hComPort,
			(DWORD)hClientSock, 0);

		businessService->getIocpService()->Recv(hClientSock);

		// 초기 접속 메세지 Send
		string str = "접속을 환영합니다!\n";
		str += loginBeforeMessage;

		businessService->getIocpService()->SendToOneMsg(
			str.c_str(), hClientSock, ClientStatus::STATUS_LOGOUT);
	}
	
	delete businessService;

	return 0;
}