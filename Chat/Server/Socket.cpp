
#include "Socket.h"

Socket::Socket() {

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "WSAStartup() error!" << endl;
		exit(1);
	}
	
	// Overlapped IO가능 소켓을 만든다
	// TCP 통신할것
	hServSock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
		WSA_FLAG_OVERLAPPED);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = PF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY뜻은 어느 IP에서 접속이 와도 요청 수락한다는 뜻
	servAdr.sin_port = htons(atoi(SERVER_PORT));
	
	if (_WINSOCK2API_::bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR) {
		cout << "bind() error!" << endl;
		exit(1);
	}

	if (listen(hServSock, 2) == SOCKET_ERROR) {
		// listen의 두번째 인자는 접속 대기 큐의 크기
		// accept작업 다 끝나기전에 대기 할 공간
		cout << "listen() error!" << endl;
		exit(1);
	}
}

SOCKET& Socket::getSocket() {
	return hServSock;
}