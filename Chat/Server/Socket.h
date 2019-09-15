
#ifndef SOCKET_H_
#define SOCKET_H_

#include <WinSock2.h>
#include <iostream>
#include "common.h"

using namespace std;
#define MAKE_SINGLETON(className)				\
	public:												\
		static className* GetInstance()					\
		{												\
			static className instance;					\
			return &instance;							\
		}												\
														\
	private:											\
		className(const className& rhs) = delete;		\
		className(const className&& rhs) = delete;		\
		void operator=(const className& rhs) = delete;	\
		void operator=(const className&& rhs) = delete;
		
class Socket {
private:
	WSADATA wsaData;
	SOCKET hServSock;
	SOCKADDR_IN servAdr;
	Socket();
	MAKE_SINGLETON(Socket);
public:
	SOCKET& getSocket();
	virtual ~Socket();
}; 
#endif /* DAO_H_ */
