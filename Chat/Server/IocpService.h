/*
 * IocpService.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#ifndef IOCPSERVICE_H_
#define IOCPSERVICE_H_

#include <iostream>
#include <stdio.h>
#include <process.h>
#include <winsock2.h>
#include <unordered_map>
#include <list>
#include "common.h"
#include "IocpService.h"

namespace IocpService {

class IocpService {
public:
	IocpService();
	virtual ~IocpService();

	// 한명에게 메세지 전달
	void SendToOneMsg(const char *msg, SOCKET mySock, int status, LPPER_IO_DATA ioInfo);
	// 같은 방의 사람들에게 메세지 전달
	void SendToRoomMsg(const char *msg, const list<SOCKET> &lists, int status, LPPER_IO_DATA ioInfo,CRITICAL_SECTION cs);
	// Recv 계속 공통함수
	void RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo);
	// Recv 공통함수
	void Recv(SOCKET sock,LPPER_IO_DATA ioInfo);
};

} /* namespace IocpService */

#endif /* IOCPSERVICE_H_ */
