/*
 * common.h
 *
 *  Created on: 2019. 1. 8.
 *      Author: choiis1207
 */

#ifndef COMMON_H_
#define COMMON_H_

using namespace std;

#define BUF_SIZE 4096
#define NAME_SIZE 20

// CP가 Recv 받을때 READ Send 받을때 WRITE
#define READ 6
#define WRITE 7
#define READ_MORE 8

// 클라이언트 상태 정보 => 서버에서 보관할것
#define STATUS_NO_CHANGE 0
#define STATUS_LOGOUT 1
#define STATUS_WAITING 2
#define STATUS_CHATTIG 3
#define STATUS_WHISPER 4

// 서버에게 지시 사항
#define USER_MAKE 1
#define USER_ENTER 2

// 서버에 올 지시 사항
#define ROOM_MAKE 3
#define ROOM_ENTER 4
#define ROOM_OUT 5
#define WHISPER 6
#define ROOM_INFO 7
#define ROOM_USER_INFO 8
// 로그인 중복 방지
#define NOW_LOGOUT 0
#define NOW_LOGIN 1

#define loginBeforeMessage "1.계정생성 2.로그인하기 3.종료 4.콘솔지우기"
#define waitRoomMessage	"1.방 정보 보기 2.방 만들기 3.방 입장하기 4.유저 정보 5.귓속말 6.로그아웃 7.콘솔지우기"
#define errorMessage "잘못된 명령어 입니다"
#define chatRoomMessage  "나가시려면 \\out을 콘솔지우기는 \\clear를 입력하세요"

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

#endif /* COMMON_H_ */
