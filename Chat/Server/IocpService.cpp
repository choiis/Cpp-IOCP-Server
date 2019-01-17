/*
 * IocpService.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "IocpService.h"

namespace IocpService {

IocpService::IocpService() {
	// TODO Auto-generated constructor stub

}

IocpService::~IocpService() {
	// TODO Auto-generated destructor stub
}

// 한명에게 메세지 전달
void IocpService::SendToOneMsg(const char *msg, SOCKET mySock, int status, LPPER_IO_DATA ioInfo) {

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

	int len = strlen(msg) + 1;
	char *packet;
	packet = new char[len + (3 * sizeof(int))];
	memcpy(packet, &len, 4); // dataSize;
	memcpy(((char*) packet) + 4, &status, 4); // status;
	memset(((char*) packet) + 8, 0, 4); // direction;
	memcpy(((char*) packet) + 12, msg, len); // status

	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = len + (3 * sizeof(int));
	ioInfo->serverMode = WRITE; // GetQueuedCompletionStatus 이후 분기가 Send로 갈수 있게
	ioInfo->totByte = 1;
	ioInfo->recvByte = 0;
	WSASend(mySock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped),
	NULL);
}

// 같은 방의 사람들에게 메세지 전달
void IocpService::SendToRoomMsg(const char *msg, const list<SOCKET> &lists,
		int status , LPPER_IO_DATA ioInfo,CRITICAL_SECTION cs) {



	int len = strlen(msg) + 1;
	char *packet;
	packet = new char[len + (3 * sizeof(int))];
	memcpy(packet, &len, 4); // dataSize;
	memcpy(((char*) packet) + 4, &status, 4); // status;
	memset(((char*) packet) + 8, 0, 4); // direction;
	memcpy(((char*) packet) + 12, msg, len); // status

	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = len + (3 * sizeof(int));
	ioInfo->serverMode = WRITE; // GetQueuedCompletionStatus 이후 분기가 Send로 갈수 있게
	ioInfo->recvByte = 0;
	list<SOCKET>::const_iterator iter; // 변경 불가능 객체를 가리키는 반복자

	EnterCriticalSection(&cs);
	ioInfo->totByte = lists.size(); //Send io해제 시점 알기 위해
	for (iter = lists.begin(); iter != lists.end(); iter++) {
		WSASend((*iter), &(ioInfo->wsaBuf), 1,
		NULL, 0, &(ioInfo->overlapped), NULL);
	}
	LeaveCriticalSection(&cs);
}

// Recv 계속 공통함수
void IocpService::RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo) {

	DWORD recvBytes = 0;
	DWORD flags = 0;

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = SIZE;
	memset(ioInfo->buffer, 0, SIZE);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ_MORE; // GetQueuedCompletionStatus 이후 분기가 READ_MORE로 갈수 있게
	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}
// Recv 공통함수
void IocpService::Recv(SOCKET sock,LPPER_IO_DATA ioInfo) {

	DWORD recvBytes = 0;
	DWORD flags = 0;

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

} /* namespace IocpService */
