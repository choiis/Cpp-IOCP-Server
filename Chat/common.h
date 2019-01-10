/*
 * common.h
 *
 *  Created on: 2019. 1. 8.
 *      Author: choiis1207
 */

#ifndef COMMON_H_
#define COMMON_H_

using namespace std;
#define BUF_SIZE 512
#define NAME_SIZE 20

// CP가 Recv 받을때 READ Send 받을때 WRITE
#define READ 1
#define WRITE 2

// 클라이언트 상태 정보 => 서버에서 보관할것
#define STATUS_INIT 0
#define STATUS_LOGOUT 1
#define STATUS_WAITING 2
#define STATUS_CHATTIG 3
#define STATUS_WHISPER 4

// 서버에게 지시 사항
#define USER_MAKE 1
#define USER_ENTER 2
#define USER_LIST 3
#define USER_OUT 4

// 서버에 올 지시 사항
#define ROOM_MAKE 1
#define ROOM_ENTER 2
#define ROOM_OUT 3
#define WHISPER 4

// 로그인 중복 방지
#define NOW_LOGOUT 0
#define NOW_LOGIN 1

// 실제 통신 패킷 데이터
typedef struct {
	int clientStatus;
	char message[BUF_SIZE];
	char message2[NAME_SIZE];
	char message3[NAME_SIZE];
	int direction;
} PACKET_DATA, *P_PACKET_DATA;

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[sizeof(PACKET_DATA)];  // 버퍼에 PACKET_DATA가 들어갈 것이므로 크기 맞춤
	int serverMode;
} PER_IO_DATA, *LPPER_IO_DATA;
char loginBeforeMessage[] = "1.계정생성 2.로그인하기 3.계정 리스트 4.종료 5.콘솔지우기";
char waitRoomMessage[] =
		"1.방 정보 보기 2.방 만들기 3.방 입장하기 4.유저 정보 5.귓속말 6.로그아웃 7.콘솔지우기";
char errorMessage[] = "잘못된 명령어 입니다\n";
char chatRoomMessage[] = "나가시려면 out을 콘솔지우기는 clear를 입력하세요";

#endif /* COMMON_H_ */
