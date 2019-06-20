/*
 * common.h
 *
 *  Created on: 2019. 1. 8.
 *      Author: choiis1207
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <winsock2.h>
#include <list>

using namespace std;
#define BUF_SIZE 4096
#define NAME_SIZE 20
#define FILE_BUF_SIZE 32768

// 클라이언트 상태 정보 => 서버에서 보관할것
enum ClientStatus {

	STATUS_INIT,
	STATUS_LOGOUT,
	STATUS_WAITING,
	STATUS_CHATTIG,
	STATUS_WHISPER,
	STATUS_FILE_SEND,
	STATUS_MAX
};

enum Direction {
	// 서버에게 지시 사항
	USER_MAKE = 1,
	USER_ENTER,
	// 서버에 올 지시 사항
	ROOM_MAKE,
	ROOM_ENTER,
	ROOM_OUT,
	WHISPER,
	ROOM_INFO,
	ROOM_USER_INFO,
	FRIEND_INFO,
	FRIEND_ADD,
	FRIEND_GO,
	FRIEND_DELETE,
	LOG_OUT,
	FILE_SEND,
	MAX,

	// Node 관리콘솔에서 보낼 메세지
	CALLCOUNT = 20,
	BAN = 21,
	EXIT = 22
};

// 로그인 중복 방지

enum LoginCheck {
	NOW_LOGOUT,
	NOW_LOGIN,
	NOW_LOG_MAX
};

enum SendTo{
	SEND_ME = 1,
	SEND_ROOM,
	SEND_FILE,
	SEND_MAX
};

#define loginBeforeMessage "1.계정생성 2.로그인하기 3.종료 4.콘솔지우기"
#define waitRoomMessage	"1.방 정보 보기 2.방 만들기 3.방 입장하기 4.유저 정보 5.친구 정보 6.친구 관리 7.귓속말 8.로그아웃 9.콘솔지우기"
#define errorMessage "잘못된 명령어 입니다"
#define chatRoomMessage  "나가시려면 \\out을 콘솔지우기는 \\clear를 친구추가는 \\add 닉네임을 파일전송은\\send를 입력하세요"


#define SERVER_PORT "1234"
#define UDP_PORT "1236"
#define UDP_PORT_SEND "2222"

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];  // 512Byte
	int serverMode;
	short recvByte; // 지금까지 받은 바이트를 알려줌
	short totByte; //전체 받을 바이트를 알려줌
	short bodySize; // 패킷의 body 사이즈
	char *recvBuffer;
} PER_IO_DATA, *LPPER_IO_DATA;

// JOB Queue에 대한 구조체
typedef struct { // buffer info
	SOCKET socket;
	int direction;
	int nowStatus;
	string msg;
	int job;
} JOB_DATA, *P_JOB_DATA;

// Send Queue에 대한 구조체
typedef struct { // buffer info
	string msg;
	SOCKET mySocket;
	string roomName;
	SendTo direction;
	ClientStatus status;
} Send_DATA, *P_Send_DATA;

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	list<SOCKET> userList;
	CRITICAL_SECTION listCs;
} ROOM_DATA, *P_ROOM_DATA;


#endif /* COMMON_H_ */
