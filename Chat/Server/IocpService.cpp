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
void IocpService::SendToOneMsg(const char *msg, SOCKET mySock, int status) {
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->Malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

	int len = min((int) strlen(msg) + 1, BUF_SIZE - 12); // 최대 보낼수 있는 내용 500Byte
	CharPool* charPool = CharPool::getInstance();
	char* packet = charPool->Malloc(); // 512 Byte까지 읽기 가능

	copy((char*) &len, (char*) &len + 4, packet); // dataSize
	copy((char*) &status, (char*) &status + 4, packet + 4);  // status
	copy(msg, msg + len, packet + 12);  // msg
	memset(((char*) packet) + 8, 0, 4); // direction;

	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = min(len + 12, BUF_SIZE);
	ioInfo->serverMode = WRITE;
	ioInfo->totByte = 1;
	ioInfo->recvByte = 0;

	WSASend(mySock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped),
	NULL);

}
// 같은 방의 사람들에게 메세지 전달
void IocpService::SendToRoomMsg(const char *msg, const list<SOCKET> &lists,
		int status, CRITICAL_SECTION *listCs) {
	MPool* mp = MPool::getInstance();
	CharPool* charPool = CharPool::getInstance();

	list<SOCKET>::const_iterator iter; // 변경 불가능 객체를 가리키는 반복자

	EnterCriticalSection(listCs);
	for (iter = lists.begin(); iter != lists.end(); iter++) {
		// ioInfo를 각개 만들어서 보내자
		LPPER_IO_DATA ioInfo = mp->Malloc();
		int len = min((int)strlen(msg) + 1, BUF_SIZE - 12); // 최대 보낼수 있는 내용 500Byte

		char* packet = charPool->Malloc(); // 512 Byte까지 읽기 가능

		copy((char*) &len, (char*) &len + 4, packet); // dataSize
		copy((char*) &status, (char*) &status + 4, packet + 4);  // status
		copy(msg, msg + len, packet + 12);  // msg
		memset(((char*) packet) + 8, 0, 4); // direction;

		ioInfo->wsaBuf.buf = (char*) packet;
		ioInfo->wsaBuf.len = min(len + 12, BUF_SIZE);
		ioInfo->serverMode = WRITE;
		ioInfo->recvByte = 0;

		WSASend((*iter), &(ioInfo->wsaBuf), 1,
		NULL, 0, &(ioInfo->overlapped), NULL);
	}
	LeaveCriticalSection(listCs);

}
// Recv 계속 공통함수
void IocpService::RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo) {
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
void IocpService::Recv(SOCKET sock) {
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->Malloc();

	DWORD recvBytes = 0;
	DWORD flags = 0;

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

string IocpService::GetNowTime() {
	string str;
	char date[20];
	struct timeb itb;
	struct tm *lt;
	ftime(&itb);
	lt = localtime(&itb.time);
	// format : YYMMDDhhmmssuuuuuu
	sprintf(date, "%04d%02d%02d%02d%02d%02d%03d", lt->tm_year + 1900,
			lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec,
			itb.millitm);
	str = string(date);
	return str;
}


}
/* namespace IocpService */
