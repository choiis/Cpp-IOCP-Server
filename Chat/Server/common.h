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
#include <mutex>
using namespace std;

#define BUF_SIZE 4096
#define NAME_SIZE 20
#define FILE_BUF_SIZE 32768

typedef unsigned short uint2;

// Ŭ���̾�Ʈ ���� ���� => �������� �����Ұ�
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
	// �������� ���� ����
	USER_MAKE = 1,
	USER_ENTER,
	// ������ �� ���� ����
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
	USER_GOOD,
	USER_GOOD_INFO,
	MAX,

	// Node �����ֿܼ��� ���� �޼���
	CALLCOUNT = 20,
	BAN = 21,
	EXIT = 22
};

// �α��� �ߺ� ����

enum class LoginCheck {
	NOW_LOGOUT,
	NOW_LOGIN,
	NOW_LOG_MAX
};

enum class SendTo {
	SEND_ME = 1,
	SEND_ROOM,
	SEND_FILE,
	SEND_MAX
};

#define loginBeforeMessage "1.�������� 2.�α����ϱ� 3.���� 4.�ܼ������"
#define waitRoomMessage	"1.�� ���� ���� 2.�� ����� 3.�� �����ϱ� 4.���� ���� 5.ģ�� ���� 6.ģ�� ���� 7.�ӼӸ� 8.�α׾ƿ� 10.�ܼ������"
#define errorMessage "�߸��� ��ɾ� �Դϴ�"
#define chatRoomMessage  "�����÷��� \\out�� �ܼ������� \\clear�� ģ���߰��� \\add �г����� ����������\\send�� �Է��ϼ���"


#define SERVER_PORT "1234"
#define UDP_PORT "1236"
#define UDP_PORT_SEND "2222"

// �񵿱� ��ſ� �ʿ��� ����ü
typedef struct { // buffer info
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];  // 512Byte
	int serverMode;
	short recvByte; // ���ݱ��� ���� ����Ʈ�� �˷���
	short totByte; //��ü ���� ����Ʈ�� �˷���
	short bodySize; // ��Ŷ�� body ������
	char *recvBuffer;
} PER_IO_DATA, *LPPER_IO_DATA;

// JOB Queue�� ���� ����ü
typedef struct { // buffer info
	SOCKET socket;
	Direction direction;
	ClientStatus nowStatus;
	string msg;
	int job;
} JOB_DATA, *P_JOB_DATA;

// Send Queue�� ���� ����ü
typedef struct { // buffer info
	string msg;
	SOCKET mySocket;
	string roomName;
	SendTo direction;
	ClientStatus status;
} Send_DATA, *P_Send_DATA;

// �񵿱� ��ſ� �ʿ��� ����ü
typedef struct { // buffer info
	list<SOCKET> userList;
	recursive_mutex listCs;
} ROOM_DATA, *P_ROOM_DATA;


#endif /* COMMON_H_ */
