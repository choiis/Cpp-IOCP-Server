/*
 * IocpService.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#ifndef IOCPSERVICE_H_
#define IOCPSERVICE_H_

#include <winsock2.h>
#include <algorithm>
#include <list>
#include "common.h"
#include "MPool.h"
#include "CharPool.h"

namespace IocpService {

class IocpService {
private:
	IocpService(const IocpService& rhs) = delete;
	void operator=(const IocpService& rhs) = delete;
public:
	enum Operation {
		RECV = 1,
		SEND = 2,
		RECV_MORE = 3
	};

	IocpService();
	virtual ~IocpService();

	// 한명에게 메세지 전달
	void SendToOneMsg(const char *msg, SOCKET mySock, int status);
	// 같은 방의 사람들에게 메세지 전달
	void SendToRoomMsg(const char *msg, const list<SOCKET> &lists, int status);
	// Recv 계속 공통함수
	void RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo);

	// Recv 공통함수
	void Recv(SOCKET sock);

};
/* namespace IocpService */
}
#endif /* IOCPSERVICE_H_ */
