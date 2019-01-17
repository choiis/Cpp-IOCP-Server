//============================================================================
// Name        : Client.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>
#include <regex>
#include "common.h"

// ���� Ŭ���̾�Ʈ ���� => ���� => ���� �α��� �������� �ٲ��
int clientStatus;

MPool mp(100);

using namespace std;

typedef struct { // socket info
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

// ����+���� ���ڿ� �Ǻ�
bool IsAlphaNumber(string str) {
	regex alpha("[a-z|A-Z|0-9]+");
	if (regex_match(str, alpha)) {
		return true;
	} else {
		return false;
	}
}

// Recv ��� �����Լ�
void RecvMore(SOCKET sock, LPPER_IO_DATA ioInfo) {
	DWORD recvBytes = 0;
	DWORD flags = 0;
	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = SIZE;
	memset(ioInfo->buffer, 0, SIZE);
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ_MORE; // GetQueuedCompletionStatus ���� �бⰡ Recv�� ���� �ְ�

	// ��� Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}
// Recv �����Լ�
void Recv(SOCKET sock) {
	DWORD recvBytes = 0;
	DWORD flags = 0;
	//LPPER_IO_DATA ioInfo = new PER_IO_DATA;
	LPPER_IO_DATA ioInfo = mp.malloc();

	memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
	ioInfo->wsaBuf.len = SIZE;
	ioInfo->wsaBuf.buf = ioInfo->buffer;
	ioInfo->serverMode = READ; // GetQueuedCompletionStatus ���� �бⰡ Recv�� ���� �ְ�
	ioInfo->recvByte = 0;
	ioInfo->totByte = 0;

	// ��� Recv
	WSARecv(sock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags,
			&(ioInfo->overlapped),
			NULL);
}

// fgets�� \n���� �����Լ� ���� �����÷ο����
void Gets(char *message, int size) {
	fgets(message, size, stdin);
//	string str;
//	getline(cin, str);
	char *p;
	if ((p = strchr(message, '\n')) != NULL) {
		*p = '\0';
	}
}

// WSASend�� call
void SendMsg(SOCKET clientSock, const char* msg, int status, int direction) {
	LPPER_IO_DATA ioInfo = new PER_IO_DATA;

	WSAOVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(OVERLAPPED));

	int len = strlen(msg) + 1;
	char *packet;
	packet = new char[len + (3 * sizeof(int))];
	memcpy(packet, &len, 4); // dataSize;
	memcpy(((char*) packet) + 4, &clientStatus, 4); // status;
	memcpy(((char*) packet) + 8, &direction, 4); // direction;
	memcpy(((char*) packet) + 12, msg, len); // status;

	ioInfo->wsaBuf.buf = (char*) packet;
	ioInfo->wsaBuf.len = len + (3 * sizeof(int));
	ioInfo->serverMode = WRITE;

	WSASend(clientSock, &(ioInfo->wsaBuf), 1, NULL, 0, &overlapped,
	NULL);

}

// �۽��� ����� ������
unsigned WINAPI SendMsgThread(void *arg) {
	// �Ѿ�� clientSocket�� �޾���
	SOCKET clientSock = *((SOCKET*) arg);

	while (1) {
		string msg;
		getline(cin, msg);

		int direction = -1;
		int status;

		if (clientStatus == STATUS_LOGOUT) { // �α��� ����
			if (msg.compare("1") == 0) {	// ���� ���� ��

				while (true) {
					cout << "������ �Է��� �ּ��� (��������10�ڸ�)" << endl;
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
					cout << "��й�ȣ�� �Է��� �ּ��� (��������4�ڸ��̻� 10�ڸ�����)" << endl;
					getline(cin, password1);
					if (password1.compare("") == 0) {
						continue;
					} else if (password1.length() >= 4
							&& password1.length() <= 10
							&& IsAlphaNumber(password1)) {
						break;
					}
				}

				while (true) {
					cout << "��й�ȣ�� �ѹ��� �Է��� �ּ���" << endl;
					getline(cin, password2);

					if (password2.compare("") == 0) {
						continue;
					}

					if (password1.compare(password2) != 0) { // ��й�ȣ �ٸ�
						cout << "��й�ȣ Ȯ�� ����!" << endl;
					} else {
						break;
					}
				}

				while (true) {
					cout << "�г����� �Է��� �ּ��� (20����Ʈ ����)" << endl;
					getline(cin, nickname);

					if (nickname.compare("") != 0 && nickname.length() <= 20) {
						break;
					}
				}
				msg.append("\\");
				msg.append(password1); // ��й�ȣ
				msg.append("\\");
				msg.append(nickname); // �г���
				direction = USER_MAKE;
				status = STATUS_LOGOUT;
			} else if (msg.compare("2") == 0) { // �α��� �õ�
				cout << "������ �Է��� �ּ���" << endl;
				getline(cin, msg);

				string password;

				cout << "��й�ȣ�� �Է��� �ּ���" << endl;
				getline(cin, password);

				msg.append("\\");
				msg.append(password); // ��й�ȣ

				direction = USER_ENTER;
				status = STATUS_LOGOUT;
			} else if (msg.compare("3") == 0) {
				direction = USER_LIST;
				status = STATUS_LOGOUT;
			} else if (msg.compare("4") == 0) { // Ŭ���̾�Ʈ ��������
				exit(1);
			} else if (msg.compare("5") == 0) { // �ܼ������
				system("cls");
				cout << loginBeforeMessage << endl;
				continue;
			}
		} else if (clientStatus == STATUS_WAITING) { // ���� �� ��
			if (msg.compare("1") == 0) {	// �� ���� ��
				direction = ROOM_INFO;
			} else if (msg.compare("2") == 0) {	// �� ���� ��
				cout << "�� �̸��� �Է��� �ּ���" << endl;
				getline(cin, msg);

				direction = ROOM_MAKE;
			} else if (msg.compare("3") == 0) {	// �� ������ ��
				cout << "������ �� �̸��� �Է��� �ּ���" << endl;
				getline(cin, msg);
				direction = ROOM_ENTER;
			} else if (msg.compare("4") == 0) {	// �� ���� ��
				direction = ROOM_USER_INFO;
			} else if (msg.compare("5") == 0) { // �ӼӸ�

				string Msg;
				cout << "�ӼӸ� ����� �Է��� �ּ���" << endl;
				getline(cin, msg);
				cout << "������ ���� �Է��� �ּ���" << endl;
				getline(cin, Msg);
				msg.append("\\");
				msg.append(Msg); // ���
				direction = WHISPER;
			} else if (msg.compare("7") == 0) { // �ܼ������
				system("cls");
				cout << waitRoomMessage << endl;
				continue;
			}
		} else if (clientStatus == STATUS_CHATTIG) { // ä������ ��
			if (msg.compare("\\clear") == 0) { // �ܼ�â clear
				system("cls");
				cout << chatRoomMessage << endl;
				continue;
			}
		}

		if (msg.compare("") == 0) { // �Է¾��ϸ� Send����
			continue;
		}

		SendMsg(clientSock, msg.c_str(), status, direction);
	}
	return 0;
}

// ��Ŷ ������ �б�
void PacketReading(LPPER_IO_DATA ioInfo, DWORD bytesTrans) {
	// IO �Ϸ��� ���� �κ�
	if (READ == ioInfo->serverMode) {
		if (bytesTrans < 4) { // ����� �� �� �о�� ��Ȳ
			memcpy(((char*) &(ioInfo->bodySize)) + ioInfo->recvByte,
					ioInfo->buffer, bytesTrans);
		} else {
			memcpy(&(ioInfo->bodySize), ioInfo->buffer, 4);
			ioInfo->recvBuffer = new char[ioInfo->bodySize + 12]; // BodySize��ŭ ���� �Ҵ�
			memcpy(((char*) ioInfo->recvBuffer), ioInfo->buffer, bytesTrans);
		}
		ioInfo->recvByte += bytesTrans; // ���ݱ��� ���� ������ �� ����
	} else { // �� �б�
		if (ioInfo->recvByte >= 4) { // ��� �� �о���
			memcpy(((char*) ioInfo->recvBuffer) + ioInfo->recvByte,
					ioInfo->buffer, bytesTrans);
			ioInfo->recvByte += bytesTrans; // ���ݱ��� ���� ������ �� ����
		} else { // ����� �� ���о��� ���
			int recv = min(4 - ioInfo->recvByte, bytesTrans);
			memcpy(((char*) &(ioInfo->bodySize)) + ioInfo->recvByte,
					ioInfo->buffer, recv); // ������� ä���
			ioInfo->recvByte += bytesTrans; // ���ݱ��� ���� ������ �� ����
			if (ioInfo->recvByte >= 4) {
				ioInfo->recvBuffer = new char[ioInfo->bodySize + 12]; // BodySize��ŭ ���� �Ҵ�
				memcpy(((char*) ioInfo->recvBuffer) + 4,
						((char*) ioInfo->buffer) + recv, bytesTrans - recv); // ���� ����� �ʿ���� => �̶��� ���� �����͸� ����
			}
		}
	}

	if (ioInfo->totByte == 0 && ioInfo->recvByte >= 4) { // ����� �� �о�� ��Ż ����Ʈ ���� �� �� �ִ�
		ioInfo->totByte = ioInfo->bodySize + 12;
	}
}

// Ŭ���̾�Ʈ���� ���� ������ ������ ����ü ����
char* DataCopy(LPPER_IO_DATA ioInfo, int *status) {
	memcpy(status, ((char*) ioInfo->recvBuffer) + 4, 4); // Status
	char* msg = new char[ioInfo->bodySize];	// Msg
	memcpy(msg, ((char*) ioInfo->recvBuffer) + 12, ioInfo->bodySize);
	// �� ���� �޾����� �Ҵ� ����
	delete ioInfo->recvBuffer;
	// �޸� ����
	// delete ioInfo;
	mp.free(ioInfo);

	return msg;
}

// ������ ����� ������
unsigned WINAPI RecvMsgThread(LPVOID hComPort) {

	SOCKET sock;
	DWORD bytesTrans;
	LPPER_IO_DATA ioInfo;

	while (1) {
		GetQueuedCompletionStatus(hComPort, &bytesTrans, (LPDWORD) &sock,
				(LPOVERLAPPED*) &ioInfo, INFINITE);

		if (READ_MORE == ioInfo->serverMode || READ == ioInfo->serverMode) {

			// ������ �б� ����
			PacketReading(ioInfo, bytesTrans);

			if ((ioInfo->recvByte < ioInfo->totByte)
					|| (ioInfo->recvByte < 4 && ioInfo->totByte == 0)) { // ���� ��Ŷ ���� || ��� �� ������ -> ���޾ƾ���

				RecvMore(sock, ioInfo); // ��Ŷ ���ޱ�
			} else {
				int status;
				char *msg = DataCopy(ioInfo, &status);

				// Client�� ���� ���� ���� �ʼ�
				// �������� �ذ����� ����
				if (status == STATUS_LOGOUT || status == STATUS_WAITING
						|| status == STATUS_CHATTIG) {
					if (clientStatus != status) { // ���� ����� �ܼ� clear
						system("cls");
					}
					clientStatus = status; // clear���� client���º��� ���ش�
					cout << msg << endl;
				} else if (status == STATUS_WHISPER) { // �ӼӸ� �����϶��� Ŭ���̾�Ʈ ���º�ȭ ����
					cout << msg << endl;
				}

				Recv(sock);
			}
		}

	}
	return 0;
}
int main(int argc, char* argv[0]) {

	WSADATA wsaData;
	SOCKET hSocket;
	SOCKADDR_IN servAddr;

	HANDLE sendThread, recvThread;

	// Socket lib�� �ʱ�ȭ
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup() error!");
		exit(1);
	}
	// Overlapped IO���� ������ �����
	// TCP ����Ұ�
	hSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
	WSA_FLAG_OVERLAPPED);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = PF_INET;
	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	while (1) {

		cout << "��Ʈ��ȣ�� �Է��� �ּ��� :";
		string port;
		cin >> port;

		servAddr.sin_port = htons(atoi(port.c_str()));

		if (connect(hSocket, (SOCKADDR*) &servAddr,
				sizeof(servAddr))==SOCKET_ERROR) {
			printf("connect() error!");
		} else {
			break;
		}
	}
	// Completion Port ����
	HANDLE hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// Completion Port �� ���� ����
	CreateIoCompletionPort((HANDLE) hSocket, hComPort, (DWORD) hSocket, 0);

	// ���� ������ ����
	// ������� RecvMsg�� hComPort CP ������Ʈ�� �Ҵ��Ѵ�
	// RecvMsg���� Recv�� �Ϸ�Ǹ� ������ �κ��� �ִ�
	recvThread = (HANDLE) _beginthreadex(NULL, 0, RecvMsgThread,
			(LPVOID) hComPort, 0,
			NULL);

	// �۽� ������ ����
	// Thread�ȿ��� clientSocket���� Send���ٰŴϱ� ���ڷ� �Ѱ��ش�
	// CP�� Send�� ���� �ȵǾ����� GetQueuedCompletionStatus���� Send �Ϸ�ó�� �ʿ����
	sendThread = (HANDLE) _beginthreadex(NULL, 0, SendMsgThread,
			(void*) &hSocket, 0,
			NULL);

	Recv(hSocket);
	WaitForSingleObject(sendThread, INFINITE);
	WaitForSingleObject(recvThread, INFINITE);
	closesocket(hSocket);
	WSACleanup();
	return 0;
}