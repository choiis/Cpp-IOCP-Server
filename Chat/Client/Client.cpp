//============================================================================
// Name        : Client.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <regex>
#include <algorithm>
#include <direct.h>
#include "MPool.h"
#include "CharPool.h"

// 현재 클라이언트 상태 => 대기실 => 추후 로그인 이전으로 바뀔것
int clientStatus;

using namespace std;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	SOCKET hClntSock;
	SOCKET udpSock;
	SOCKADDR_IN sockaddr_in;
} SOCKET_DATA, *P_SOCKET_DATA;

// 영문+숫자 문자열 판별
bool IsAlphaNumber(string str) {
	regex alpha("[a-z|A-Z|0-9]+");
	if (regex_match(str, alpha)) {
		return true;
	}
	else {
		return false;
	}
}

// Recv 계속 공통함수
void RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo) {
	DWORD recvBytes = 0;
	DWORD flags = 0;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = BUF_SIZE;
	memset(ioInfo->buffer, 0, BUF_SIZE);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ_MORE; // GetQueuedCompletionStatus 이후 분기가 Recv로 갈수 있게

	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
		&(ioInfo->overlapped),
		NULL);
}

// Recv 공통함수
void Recv(SOCKET sock) {
	DWORD recvBytes = 0;
	DWORD flags = 0;
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->Malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = BUF_SIZE;
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ; // GetQueuedCompletionStatus 이후 분기가 Recv로 갈수 있게
	ioInfo->recvByte = 0;
	ioInfo->totByte = 0;

	// 계속 Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
		&(ioInfo->overlapped),
		NULL);
}

// WSASend를 call
void SendMsg(SOCKET clientSock, const char* msg, int status, int direction) {
	MPool* mp = MPool::getInstance();
	LPPER_IO_DATA ioInfo = mp->Malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));

	unsigned short len = min((unsigned short)strlen(msg) + 11, BUF_SIZE); // 최대 보낼수 있는 내용 502Byte
	CharPool* charPool = CharPool::getInstance();
	char* packet = charPool->Malloc(); // 512 Byte까지 읽기 가능

	copy((char*)&len, (char*)&len + 2, packet); // dataSize
	copy((char*)&status, (char*)&status + 4, packet + 2);  // status
	copy((char*)&direction, (char*)&direction + 4, packet + 6);  // direction
	copy(msg, msg + len, packet + 10);  // msg
	ioInfo->wsaBuf.buf = (char*)packet;
	ioInfo->wsaBuf.len = len;
	ioInfo->serverMode = WRITE;

	WSASend(clientSock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped),
		NULL);

}

void FileSend(SOCKET socket, FILE* fp, string fileDir) {

	int idx;
	while ((idx = fileDir.find("/")) != -1) { // 파일명만 추출
		fileDir.erase(0, idx + 1);
	} 

	char buf[FILE_BUF_SIZE];
	fseek(fp, 0, SEEK_END);
	int file_size = ftell(fp); // 파일크기
	fseek(fp, 0, SEEK_SET);
	int totalSendBytes = 0; // 총 바이트 수
	
	char fileName[BUF_SIZE];
	strncpy(fileName, fileDir.c_str(), BUF_SIZE);
	cout << fileName << " 파일 전송을 시작합니다 " << endl;
	// 파일이름 전송		
	int sendBytes = send(socket, fileName, strlen(fileName), 0);
		
	while (true) {
		sendBytes = fread(buf, sizeof(char), FILE_BUF_SIZE, fp);
		send(socket, buf, sendBytes, 0);
		if (sendBytes == EOF || sendBytes == 0) {
			break;
		}
		else {
			Sleep(1);
		}	
	}
	cout << fileName << " 파일 " << file_size << " Byte가 서버로 전달되었습니다" << endl;
}

// 경로 표시 한가지로 통일
void replaceFileDir(string& fileDir) {
	replace(fileDir.begin(), fileDir.end(), '\\', '/');
}

// 송신을 담당할 스레드
unsigned WINAPI SendMsgThread(void *arg) {
	// 넘어온 clientSocket을 받아줌
	SOCKET_DATA sockData = *((SOCKET_DATA*)arg);
	SOCKET clientSock = sockData.hClntSock;
	SOCKET udpSocket = sockData.udpSock;
	SOCKADDR_IN udpAddr = sockData.sockaddr_in;
	
	while (1) {
		string msg;
		getline(cin, msg);

		int direction = -1;
		int status = -1;

		if (clientStatus == STATUS_LOGOUT) { // 로그인 이전
			if (msg.compare("1") == 0) {	// 계정 만들 때

				while (true) {
					cout << "계정을 입력해 주세요 (영문숫자10자리)" << endl;
					getline(cin, msg);

					if (msg.compare("") != 0 && msg.length() <= 10
						&& IsAlphaNumber(msg)) {
						break;
					}
				}

				string password1;
				string password2;

				string nickname;
				while (true) {
					cout << "비밀번호를 입력해 주세요 (영문숫자4자리이상 10자리이하)" << endl;
					getline(cin, password1);
					if (password1.compare("") == 0) {
						continue;
					}
					else if (password1.length() >= 4
						&& password1.length() <= 10
						&& IsAlphaNumber(password1)) {
						break;
					}
				}

				while (true) {
					cout << "비밀번호를 한번더 입력해 주세요" << endl;
					getline(cin, password2);

					if (password2.compare("") == 0) {
						continue;
					}

					if (password1.compare(password2) != 0) { // 비밀번호 다름
						cout << "비밀번호 확인 실패!" << endl;
					}
					else {
						break;
					}
				}

				while (true) {
					cout << "닉네임을 입력해 주세요 (20바이트 이하)" << endl;
					getline(cin, nickname);

					if (nickname.compare("") != 0 && nickname.length() <= 20) {
						break;
					}
				}
				msg.append("\\");
				msg.append(password1); // 비밀번호
				msg.append("\\");
				msg.append(nickname); // 닉네임
				direction = USER_MAKE;
				status = STATUS_LOGOUT;
			}
			else if (msg.compare("2") == 0) { // 로그인 시도
				cout << "계정을 입력해 주세요" << endl;
				getline(cin, msg);

				string password;

				cout << "비밀번호를 입력해 주세요" << endl;
				getline(cin, password);

				msg.append("\\");
				msg.append(password); // 비밀번호

				direction = USER_ENTER;
				status = STATUS_LOGOUT;
			}
			else if (msg.compare("3") == 0) { // 클라이언트 강제종료
				exit(1);
			}
			else if (msg.compare("4") == 0) { // 콘솔지우기
				system("cls");
				cout << loginBeforeMessage << endl;
				continue;
			}
		}
		else if (clientStatus == STATUS_WAITING) { // 대기실 일 때
			if (msg.compare("1") == 0) {	// 방 정보 요청
				direction = ROOM_INFO;
			}
			else if (msg.compare("2") == 0) {	// 방 만들 때
				cout << "방 이름을 입력해 주세요" << endl;
				getline(cin, msg);

				direction = ROOM_MAKE;
			}
			else if (msg.compare("3") == 0) {	// 방 입장할 때
				cout << "입장할 방 이름을 입력해 주세요" << endl;
				getline(cin, msg);
				direction = ROOM_ENTER;
			}
			else if (msg.compare("4") == 0) { // 유저 정보 요청
				direction = ROOM_USER_INFO;
			}
			else if (msg.compare("5") == 0) { // 친구 정보 요청
				direction = FRIEND_INFO;
			}
			else if (msg.compare("6") == 0) { // 친구관리
				cout << "1.친구 추가 2.친구에게 가기 3.친구 삭제 " << endl;
				string direct;
				getline(cin, direct);
				if (direct.compare("1") == 0) {
					cout << "함께할 친구 닉네임을 입력해 주세요" << endl;
					getline(cin, msg);
					direction = FRIEND_ADD;
				}
				if (direct.compare("2") == 0) { // 친구에게 가기
					cout << "함께할 친구 닉네임을 입력해 주세요" << endl;
					getline(cin, msg);
					direction = FRIEND_GO;
				}
				else if (direct.compare("3") == 0) { // 친구 삭제
					cout << "삭제할 친구 닉네임을 입력해 주세요" << endl;
					getline(cin, msg);
					direction = FRIEND_DELETE;
				}
			}
			else if (msg.compare("7") == 0) { // 귓속말

				string Msg;
				cout << "귓속말 대상을 입력해 주세요" << endl;
				getline(cin, msg);
				cout << "전달할 말을 입력해 주세요" << endl;
				getline(cin, Msg);
				msg.append("\\");
				msg.append(Msg); // 대상
				direction = WHISPER;
			}
			else if (msg.compare("8") == 0) { // 로그아웃
				direction = LOG_OUT;
			}
			else if (msg.compare("9") == 0) { // 콘솔지우기
				system("cls");
				cout << waitRoomMessage << endl;
				continue;
			}
		}
		else if (clientStatus == STATUS_CHATTIG) { // 채팅중일 때
			if (msg.compare("\\clear") == 0) { // 콘솔창 clear
				system("cls");
				cout << chatRoomMessage << endl;
				continue;
			}
			else if (msg.find("\\send") != -1) { // 파일전송
				cout << "파일 경로를 입력해 주세요" << endl;
				string fileDir;
				getline(cin, fileDir);
				replaceFileDir(fileDir);

				FILE* fp = fopen(fileDir.c_str(), "rb");
				if (fp == nullptr) {
					cout << "존재하지 않는 파일입니다" << endl;
				}
				else {
					SendMsg(clientSock, "", status, FILE_SEND);
					FileSend(udpSocket, fp, fileDir);
					fclose(fp); // 파일닫아줌
				}
				continue;
			}
		}

		if (msg.compare("") == 0) { // 입력안하면 Send안함
			continue;
		}

		SendMsg(clientSock, msg.c_str(), status, direction);
	}
	return 0;
}

// 패킷 데이터 읽기
short PacketReading(LPPER_IO_DATA ioInfo, short bytesTrans) {
	// IO 완료후 동작 부분
	if (READ == ioInfo->serverMode) {
		if (bytesTrans >= 2) {
			copy(ioInfo->buffer, ioInfo->buffer + 2,
				(char*)&(ioInfo->bodySize));
			CharPool* charPool = CharPool::getInstance();
			ioInfo->recvBuffer = charPool->Malloc(); // 512 Byte까지 카피 가능
			if ((bytesTrans - ioInfo->bodySize > 0) && (bytesTrans - ioInfo->bodySize <= BUF_SIZE)) { // 패킷 뭉쳐있는 경우
				copy(ioInfo->buffer, ioInfo->buffer + ioInfo->bodySize,
					((char*)ioInfo->recvBuffer));

				copy(ioInfo->buffer + ioInfo->bodySize, ioInfo->buffer + bytesTrans, ioInfo->buffer); // 다음에 또 쓸 byte배열 복사
				return bytesTrans - ioInfo->bodySize; // 남은 바이트 수
			}
			else if (bytesTrans - ioInfo->bodySize == 0) { // 뭉쳐있지 않으면 remainByte 0
				copy(ioInfo->buffer, ioInfo->buffer + ioInfo->bodySize,
					((char*)ioInfo->recvBuffer));
				return 0;
			}
			else  if (bytesTrans - ioInfo->bodySize < 0) { // 바디 내용 부족 => RecvMore에서 받을 부분 복사
				copy(ioInfo->buffer, ioInfo->buffer + bytesTrans, ((char*)ioInfo->recvBuffer));
				ioInfo->recvByte = bytesTrans;
				return bytesTrans - ioInfo->bodySize;
			}
			else {
				return 0;
			}
		}
		else if (bytesTrans == 1) { // 헤더 부족
			copy(ioInfo->buffer, ioInfo->buffer + bytesTrans,
				(char*)&(ioInfo->bodySize)); // 가지고있는 Byte까지 카피
			ioInfo->recvByte = bytesTrans;
			ioInfo->totByte = 0;
			return -1;
		}
		else {
			return 0;
		}
	}
	else { // 더 읽기 (BodySize짤린경우)
		ioInfo->serverMode = READ;
		if (ioInfo->recvByte < 2) { // Body정보 없음
			copy(ioInfo->buffer, ioInfo->buffer + (2 - ioInfo->recvByte),
				(char*)&(ioInfo->bodySize) + ioInfo->recvByte); // 가지고있는 Byte까지 카피
			CharPool* charPool = CharPool::getInstance();
			ioInfo->recvBuffer = charPool->Malloc(); // 512 Byte까지 카피 가능
			copy(ioInfo->buffer + (2 - ioInfo->recvByte), ioInfo->buffer + (ioInfo->bodySize + 2 - ioInfo->recvByte),
				((char*)ioInfo->recvBuffer) + 2);

			if (bytesTrans - ioInfo->bodySize > 0) { // 패킷 뭉쳐있는 경우
				copy(ioInfo->buffer + (ioInfo->bodySize - ioInfo->recvByte), ioInfo->buffer + bytesTrans, ioInfo->buffer); // 여기문제?
				return bytesTrans - (ioInfo->bodySize - ioInfo->recvByte); // 남은 바이트 수  여기문제?
			}
			else { // 뭉쳐있지 않으면 remainByte 0
				return 0;
			}
		}
		else { // body정보는 있음
			copy(ioInfo->buffer, ioInfo->buffer + (ioInfo->bodySize - ioInfo->recvByte),
				((char*)ioInfo->recvBuffer) + ioInfo->recvByte);

			// cout << "bodySize readmore" << ioInfo->bodySize << endl;

			if (bytesTrans - ioInfo->bodySize > 0) { // 패킷 뭉쳐있는 경우
				copy(ioInfo->buffer + (ioInfo->bodySize - ioInfo->recvByte), ioInfo->buffer + bytesTrans, ioInfo->buffer);
				return bytesTrans - (ioInfo->bodySize - ioInfo->recvByte); // 남은 바이트 수
			}
			else if (bytesTrans - ioInfo->bodySize == 0) { // 뭉쳐있지 않으면 remainByte 0
				return 0;
			}
			else { // 바디 내용 부족 => RecvMore에서 받을 부분 복사
				copy(ioInfo->buffer + 2, ioInfo->buffer + bytesTrans, ioInfo->buffer);
				ioInfo->recvByte = bytesTrans;
				return bytesTrans - (ioInfo->bodySize - ioInfo->recvByte);
			}
		}

	}
}

// 클라이언트에게 받은 데이터 복사후 구조체 해제
char* DataCopy(LPPER_IO_DATA ioInfo, int *status) {
	copy(((char*)ioInfo->recvBuffer) + 2, ((char*)ioInfo->recvBuffer) + 6,
		(char*)status);
	CharPool* charPool = CharPool::getInstance();
	char* msg = charPool->Malloc(); // 512 Byte까지 카피 가능

	copy(((char*)ioInfo->recvBuffer) + 10,
		((char*)ioInfo->recvBuffer) + 10
		+ min(ioInfo->bodySize, (DWORD)BUF_SIZE), msg); //에러위치

	// 다 복사 받았으니 할당 해제
	charPool->Free(ioInfo->recvBuffer);

	return msg;
}


//  파일 수신 스레드
unsigned WINAPI RecvFileThread(LPVOID arg) {

	SOCKET udpRecvSocket = *((SOCKET*)arg);

	SOCKADDR_IN servAdr;
	int servAdrSize = sizeof(servAdr);
	while (true) {
		
		int readBytes;
		long totalReadBytes = 0;

		char fileName[BUF_SIZE];// 두번째로 파일 이름을 받는다
		readBytes = recvfrom(udpRecvSocket, fileName, BUF_SIZE, 0, (SOCKADDR*)&servAdr, &servAdrSize);
		fileName[readBytes] = '\0';
		// char fileDir[BUF_SIZE] = "C:/Users/choiis1207/Dsasownloads/"; // 다운로드 폴더로 경로 지정

		char fileDir[BUF_SIZE] = "Downloads"; // 다운로드 폴더 경로생성
		char buf[FILE_BUF_SIZE];
		FILE * fp;
		mkdir(fileDir);
		strcat(fileDir, "/");
		strcat(fileDir, fileName);
		fp = fopen(fileDir, "wb");

		while (true) {
			readBytes = recvfrom(udpRecvSocket, buf, FILE_BUF_SIZE, 0, (SOCKADDR*)&servAdr, &servAdrSize);
			totalReadBytes += readBytes;// totalByte계산하기
			if (readBytes == 0 || readBytes == EOF) {
				break;
			}
			fwrite(buf, sizeof(char), readBytes, fp);
		}
		fclose(fp);
		cout << "recv file name " << fileName << " " << totalReadBytes << " Byte 다운로드 완료" << endl;
	}

	return 0;
}
// 수신을 담당할 스레드
unsigned WINAPI RecvMsgThread(LPVOID pCompPort) {

	SOCKET sock;
	short bytesTrans;
	LPPER_IO_DATA ioInfo;
	HANDLE hComPort = (HANDLE)pCompPort;

	// 넘어온 clientSocket을 받아줌

	while (true) {
		bool success = GetQueuedCompletionStatus(hComPort, (LPDWORD)&bytesTrans,
			(LPDWORD)&sock, (LPOVERLAPPED*)&ioInfo, INFINITE);

		if (bytesTrans == 0 && !success) { // 접속 끊김 콘솔 강제 종료
			// 서버 콘솔 강제종료 처리
			cout << "서버 종료" << endl;
			closesocket(sock);
			MPool* mp = MPool::getInstance();
			mp->Free(ioInfo);
		}
		else if (READ_MORE == ioInfo->serverMode
			|| READ == ioInfo->serverMode) {

			// 데이터 읽기 과정
			short remainByte = min(bytesTrans, BUF_SIZE); // 초기 Remain Byte
			bool recvMore = false;

			while (1) {
				remainByte = PacketReading(ioInfo, remainByte);

				// 다 받은 후 정상 로직
				// DataCopy내에서 사용 메모리 전부 반환
				if (remainByte >= 0) {
					int status;
					char *msg = DataCopy(ioInfo, &status);
					// Client의 상태 정보 갱신 필수
					// 서버에서 준것으로 갱신
					if (status == STATUS_LOGOUT || status == STATUS_WAITING
						|| status == STATUS_CHATTIG) {
						if (clientStatus != status) { // 상태 변경시 콘솔 clear
							system("cls");
							switch (status) {
							case STATUS_LOGOUT:
								SetConsoleTextAttribute(
									GetStdHandle(STD_OUTPUT_HANDLE), 10);
								break;
							case STATUS_WAITING:
								SetConsoleTextAttribute(
									GetStdHandle(STD_OUTPUT_HANDLE), 9);
								break;
							case STATUS_CHATTIG:
								SetConsoleTextAttribute(
									GetStdHandle(STD_OUTPUT_HANDLE), 14);
								break;
							}

						}

						clientStatus = status; // clear이후 client상태변경 해준다
						cout << msg << endl;
					}
					else if (status == STATUS_WHISPER) { // 귓속말 상태일때는 클라이언트 상태변화 없음
						cout << msg << endl;
					}
					CharPool* charPool = CharPool::getInstance();
					charPool->Free(msg);
				}

				if (remainByte == 0) {
					MPool* mp = MPool::getInstance();
					mp->Free(ioInfo);
					break;
				}
				else if (remainByte < 0) {// 받은 패킷 부족 || 헤더 다 못받음 -> 더받아야함
					RecvMore(sock, ioInfo); // 패킷 더받기 & 기본 ioInfo 보존
					recvMore = true;
					break;
				}
			}
			if (!recvMore) {
				Recv(sock);
			}
		}
		else if (WRITE == ioInfo->serverMode) {
			MPool* mp = MPool::getInstance();
			CharPool* charPool = CharPool::getInstance();
			charPool->Free(ioInfo->wsaBuf.buf);
			mp->Free(ioInfo);
		}
		else {
			MPool* mp = MPool::getInstance();
			mp->Free(ioInfo);
		}
	}
	return 0;
}

int main() {

	WSADATA wsaData;
	SOCKET hSocket, udpSocket , udpRecvSocket;
	SOCKADDR_IN servAddr;

	HANDLE sendThread, recvThread, fileThread;

	// Socket lib의 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup() error!");
		exit(1);
	}
	// Overlapped IO가능 소켓을 만든다
	// TCP 통신할것
	hSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
		WSA_FLAG_OVERLAPPED);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = PF_INET;
	servAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

	// File전송에 필요한 UDP Socket정보
	udpSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	// File수신에 필요한 UDP Socket정보
	udpRecvSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// 연결된 UDP
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
	while (1) {

		servAddr.sin_port = htons(atoi(SERVER_PORT));

		if (connect(hSocket, (SOCKADDR*)&servAddr,
			sizeof(servAddr)) == SOCKET_ERROR) {
			printf("connect() error!");
		}
		else {
			break;
		}
	}

	SOCKADDR_IN udpAddr;
	memset(&udpAddr, 0, sizeof(udpAddr));
	udpAddr.sin_family = AF_INET;
	udpAddr.sin_addr.s_addr = inet_addr(SERVER_IP); // 보낼곳 고정
	udpAddr.sin_port = htons(atoi(UDP_PORT));
	connect(udpSocket, (SOCKADDR*)&udpAddr, sizeof(udpAddr));


	SOCKADDR_IN udpRecvAddr, servAdr;
	memset(&udpRecvAddr, 0, sizeof(udpRecvAddr));
	udpRecvAddr.sin_family = AF_INET;
	udpRecvAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 받을곳
	// udpRecvAddr.sin_addr.s_addr = inet_addr(SERVER_IP); // 받을곳
	udpRecvAddr.sin_port = htons(atoi(UDP_PORT_SEND));

	bind(udpRecvSocket, (SOCKADDR*)&udpRecvAddr, sizeof(udpRecvAddr));

	// Completion Port 생성
	HANDLE hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// Completion Port 와 소켓 연결
	CreateIoCompletionPort((HANDLE)hSocket, hComPort, (DWORD)hSocket, 0);


	// 수신 스레드 동작
	// 만들어진 RecvMsg를 hComPort CP 오브젝트에 할당한다
	// RecvMsg에서 Recv가 완료되면 동작할 부분이 있다
	recvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsgThread,
		(LPVOID)hComPort, 0,
		NULL);

	// 송신 스레드 동작
	// Thread안에서 clientSocket으로 Send해줄거니까 인자로 넘겨준다
	// CP랑 Send는 연결 안되어있음 GetQueuedCompletionStatus에서 Send 완료처리 필요없음
	SOCKET_DATA socketData = { hSocket, udpSocket, udpAddr };

	sendThread = (HANDLE)_beginthreadex(NULL, 0, SendMsgThread,
		(void*)&socketData, 0,
		NULL);
	// File Recv만을 위한 UDP 스레드
	fileThread = (HANDLE)_beginthreadex(NULL, 0, RecvFileThread,
		(LPVOID)&udpRecvSocket, 0,
		NULL); 

	Recv(hSocket);
	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(fileThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	closesocket(hSocket);
	closesocket(udpSocket);
	closesocket(udpRecvSocket);
	WSACleanup();
	return 0;
}