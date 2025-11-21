#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <direct.h>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <algorithm>

#include <winsock2.h>
#include <windows.h>

#include "Socket.h"
#include "BusinessService.h"

// 필요 시 Ws2_32.lib 링크
// #pragma comment(lib, "Ws2_32.lib")

using namespace std;

// ======================= 전역 객체 & 타입 정의 =======================

// 내부 비지니스 로직 처리 클래스
BusinessService::BusinessService businessService;

// 🔹 여기서 더 이상 JOB_DATA를 정의하지 않는다!
//    -> 기존 프로젝트의 JOB_DATA 정의를 그대로 사용
// struct JOB_DATA { ... }  <= 이거 전부 삭제

// Recv -> Work 전달용 잡 큐
queue<JOB_DATA> jobQueue;

// 큐 보호용 mutex + condition_variable
mutex queueCs;
condition_variable jobCv;   // WorkThread 깨우기용

// 패킷 카운터 (CALLCOUNT 용)
atomic<int> packetCnt = 0;

// Send 작업용 큐
struct SEND_JOB {
	SOCKET socket;
	std::string msg;
	ClientStatus status;
};

// SendThread 처리용 send 큐 & 동기화 객체
queue<SEND_JOB> sendQueue;
mutex sendCs;
condition_variable sendCv;

// ======================= Send 큐 헬퍼 함수 =======================

void EnqueueSend(const SEND_JOB& job) {
	{
		lock_guard<mutex> lock(sendCs);
		sendQueue.push(job);
	}
	sendCv.notify_one();
}

// ======================= SendThread =======================

unsigned WINAPI SendThread(void* arg) {
	UNREFERENCED_PARAMETER(arg);

	while (true) {
		queue<SEND_JOB> localQueue;

		{
			unique_lock<mutex> lock(sendCs);
			sendCv.wait(lock, [] {
				return !sendQueue.empty();
			});

			sendQueue.swap(localQueue);
		}

		while (!localQueue.empty()) {
			SEND_JOB job = std::move(localQueue.front());
			localQueue.pop();

			if (businessService.IsSocketDead(job.socket)) {
				continue;
			}

			businessService.getIocpService()->SendToOneMsg(
				job.msg.c_str(),
				job.socket,
				job.status
			);
		}
	}

	return 0;
}

// ======================= WorkThread =======================

unsigned WINAPI WorkThread(void* arg) {
	UNREFERENCED_PARAMETER(arg);

	while (true) {
		queue<JOB_DATA> localQueue;

		{
			unique_lock<mutex> lock(queueCs);

			jobCv.wait(lock, [] {
				return !jobQueue.empty();
			});

			jobQueue.swap(localQueue);
		}

		while (!localQueue.empty()) {
			JOB_DATA jobData = std::move(localQueue.front());
			localQueue.pop();

			if (businessService.IsSocketDead(jobData.socket)) {
				continue;
			}

			if (jobData.direction == Direction::CALLCOUNT) {
				int cnt = packetCnt.exchange(0);
				businessService.CallCnt(jobData.socket, cnt);
			}
			else if (jobData.direction == Direction::BAN) {
				// 🔹 nowStatus가 ClientStatus(enum)일 가능성이 높으므로 캐스팅
				businessService.BanUser(
					jobData.socket,
					jobData.msg.substr(
						0,
						static_cast<int>(jobData.nowStatus)
					).c_str()
				);
			}
			else {
				ClientStatus status = businessService.GetStatus(jobData.socket);

				businessService.callFuncPointer(
					jobData.socket,
					status,
					jobData.direction,
					jobData.msg.c_str()
				);
			}
		}
	}

	return 0;
}

// ======================= RecvThread =======================

unsigned WINAPI RecvThread(LPVOID pCompPort) {
	HANDLE hComPort = (HANDLE)pCompPort;
	SOCKET sock = INVALID_SOCKET;
	DWORD bytesTrans = 0;
	LPPER_IO_DATA ioInfo = nullptr;

	while (true) {
		BOOL success = GetQueuedCompletionStatus(
			hComPort,
			&bytesTrans,
			(PULONG_PTR)& sock,
			(LPOVERLAPPED*)& ioInfo,
			INFINITE
		);

		if (!success) {
			DWORD errorNum = WSAGetLastError();

			if (ioInfo == nullptr) {
				cout << "GetQueuedCompletionStatus failed, ioInfo == nullptr, error: " << errorNum << endl;
				continue;
			}

			switch (errorNum) {
			case ERROR_IO_PENDING:
				cout << "ERROR_IO_PENDING " << endl;
				break;
			case ERROR_NETNAME_DELETED:
				cout << "ERROR_NETNAME_DELETED " << endl;
				break;
			case ERROR_SEM_TIMEOUT:
				cout << "ERROR_SEM_TIMEOUT " << endl;
				break;
			case ERROR_OPERATION_ABORTED:
				cout << "ERROR_OPERATION_ABORTED " << endl;
				break;
			default:
				cout << "GetQueuedCompletionStatus error: " << errorNum << endl;
				break;
			}

			businessService.ClientExit(sock);
			MPool::getInstance()->Free(ioInfo);
			continue;
		}

		// 🔹 success == TRUE && bytesTrans == 0 → graceful disconnect 처리
		if (bytesTrans == 0) {
			cout << "Client graceful disconnect" << endl;

			businessService.ClientExit(sock);
			if (ioInfo != nullptr) {
				MPool::getInstance()->Free(ioInfo);
			}
			continue;
		}

		if (businessService.getIocpService()->RECV == ioInfo->serverMode
			|| businessService.getIocpService()->RECV_MORE == ioInfo->serverMode) {

			short remainByte = static_cast<short>(min<DWORD>(bytesTrans, BUF_SIZE));
			bool recvMore = false;

			queue<JOB_DATA> packetQueue;

			while (true) {
				remainByte = businessService.PacketReading(ioInfo, remainByte);

				if (remainByte >= 0) {
					JOB_DATA jobData;
					// 🔹 여기서 nowStatus 타입은 원래 정의 그대로 (ClientStatus일 것)
					jobData.msg = businessService.DataCopy(
						ioInfo,
						&jobData.nowStatus,
						&jobData.direction
					);
					jobData.socket = sock;
					packetQueue.push(std::move(jobData));
				}

				if (remainByte == 0) {
					MPool::getInstance()->Free(ioInfo);
					break;
				}
				else if (remainByte < 0) {
					businessService.getIocpService()->RecvMore(sock, ioInfo);
					recvMore = true;
					break;
				}
			}

			{
				lock_guard<mutex> guard(queueCs);

				packetCnt.fetch_add(
					static_cast<int>(packetQueue.size()),
					std::memory_order_relaxed
				);

				while (!packetQueue.empty()) {
					JOB_DATA jobData = std::move(packetQueue.front());
					packetQueue.pop();
					jobQueue.push(std::move(jobData));
				}
			}
			jobCv.notify_all();

			if (!recvMore) {
				businessService.getIocpService()->Recv(sock);
			}
		}
		else if (businessService.getIocpService()->SEND == ioInfo->serverMode) {
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

// ======================= main =======================

int main(int argc, char* argv[]) {
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);

	HANDLE hComPort;
	SYSTEM_INFO sysInfo;

	SOCKET hServSock = Socket::GetInstance()->getSocket();

	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE), 10);

	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);

	cout << "Server ready listen" << endl;
	cout << "port number : " << SERVER_PORT << endl;

	int process = static_cast<int>(sysInfo.dwNumberOfProcessors);
	cout << "Server CPU num : " << process << endl;

	Dao::GetInstance();
	MPool* mp = MPool::getInstance();
	CharPool* charPool = CharPool::getInstance();
	(void)mp;
	(void)charPool;

	for (int i = 0; i < process; i++) {
		_beginthreadex(NULL, 0, RecvThread, (LPVOID)hComPort, 0, NULL);
	}

	for (int i = 0; i < 2 * process; i++) {
		_beginthreadex(NULL, 0, WorkThread, NULL, 0, NULL);
	}

	for (int i = 0; i < (process * 5) / 3; i++) {
		_beginthreadex(NULL, 0, SendThread, NULL, 0, NULL);
	}

	while (true) {
		SOCKET hClientSock;
		SOCKADDR_IN clntAdr;
		int addrLen = sizeof(clntAdr);
		hClientSock = accept(hServSock, (SOCKADDR*)& clntAdr, &addrLen);

		if (hClientSock == INVALID_SOCKET) {
			int err = WSAGetLastError();
			cout << "accept failed, error: " << err << endl;
			continue;
		}

		businessService.InsertLiveSocket(hClientSock, clntAdr);

		CreateIoCompletionPort(
			(HANDLE)hClientSock,
			hComPort,
			(ULONG_PTR)hClientSock,
			0
		);

		businessService.getIocpService()->Recv(hClientSock);

		string str = "접속을 환영합니다!\n";
		str += loginBeforeMessage;

		businessService.getIocpService()->SendToOneMsg(
			str.c_str(),
			hClientSock,
			ClientStatus::STATUS_LOGOUT
		);
	}

	return 0;
}
