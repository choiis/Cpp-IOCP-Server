
#ifndef SOCKET_H_
#define SOCKET_H_

#include <WinSock2.h>
#include <iostream>
#include "common.h"

using namespace std;

class Socket {
private:
	WSADATA wsaData;
	SOCKET hServSock;
	SOCKADDR_IN servAdr;

	Socket(const Socket& rhs);
	Socket& operator=(const Socket& rhs);
public:
	Socket();

	virtual ~Socket();

	SOCKET& getSocket();
}; 
#endif /* DAO_H_ */
