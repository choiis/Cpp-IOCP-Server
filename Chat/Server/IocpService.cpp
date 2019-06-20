/*
 * IocpService.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "IocpService.h"

namespace IocpService {

IocpService::IocpService() {

}

IocpService::~IocpService() {
}
// 한명에게 메세지 전달
void IocpService::SendToOneMsg(const char *msg, SOCKET mySock, ClientStatus status) {
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->Malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

	unsigned short len = min((unsigned short)strlen(msg) + 11, BUF_SIZE); // 최대 보낼수 있는 내용 502Byte
	CharPool* charPool = CharPool::getInstance();
	char* packet = charPool->Malloc(); // 512 Byte까지 보내기 가능

	copy((char*)&len, (char*)&len + 2, packet); // dataSize
	copy((char*)&status, (char*)&status + 4, packet + 2);  // status
	copy(msg, msg + len, packet + 10);  // msg
	memset(((char*)packet) + 6, 0, 4); // direction;

	ioInfo->wsaBuf.buf = (char*)packet;
	ioInfo->wsaBuf.len = len;
	ioInfo->serverMode = Operation::SEND;
	WSASend(mySock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped),
	NULL);

}
// 같은 방의 사람들에게 메세지 전달
void IocpService::SendToRoomMsg(const char *msg, const list<SOCKET> &lists,
	ClientStatus status) {
	MPool* mp = MPool::getInstance();
	CharPool* charPool = CharPool::getInstance();

	list<SOCKET>::const_iterator iter; // 변경 불가능 객체를 가리키는 반복자

	for (iter = lists.begin(); iter != lists.end(); iter++) {
		// ioInfo를 각개 만들어서 보내자
		LPPER_IO_DATA ioInfo = mp->Malloc();

		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
		unsigned short len = min((unsigned short)strlen(msg) + 11, BUF_SIZE); // 최대 보낼수 있는 내용 502Byte

		char* packet = charPool->Malloc(); // 512 Byte까지 읽기 가능

		copy((char*)&len, (char*)&len + 2, packet); // dataSize
		copy((char*)&status, (char*)&status + 4, packet + 2);  // status
		copy(msg, msg + len, packet + 10);  // msg
		memset(((char*)packet) + 6, 0, 4); // direction;

		ioInfo->wsaBuf.buf = (char*)packet;
		ioInfo->wsaBuf.len = len;
		ioInfo->serverMode = Operation::SEND;
		ioInfo->recvByte = 0;

		WSASend((*iter), &(ioInfo->wsaBuf), 1,
			NULL, 0, &(ioInfo->overlapped), NULL);
	}
}

// Recv 계속 공통함수
void IocpService::RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo) {
	DWORD recvBytes = 0;
	DWORD flags = 0;

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = BUF_SIZE;
	memset(ioInfo->buffer, 0, BUF_SIZE);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = Operation::RECV_MORE;
	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}

// Recv 공통함수
void IocpService::Recv(SOCKET sock) {
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->Malloc();

	DWORD recvBytes = 0;
	DWORD flags = 0;

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = BUF_SIZE;
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = Operation::RECV;
	ioInfo->recvByte = 0;
	ioInfo->totByte = 0;
	ioInfo->bodySize = 0;
	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}

}
/* namespace IocpService */
