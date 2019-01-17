/*
 * BusinessService.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "BusinessService.h"

namespace Service {

BusinessService::BusinessService() {
	// 임계영역 Object 생성
	InitializeCriticalSection(&cs);
	mp = new MPool(1000);
	cout<<"메모리풀 1000개 블록 할당"<<endl;
	iocpService = new IocpService::IocpService();
}

BusinessService::~BusinessService() {
	delete mp;

	delete iocpService;
	// 임계영역 Object 반환
	DeleteCriticalSection(&cs);
}

// 초기 로그인
// 세션정보 추가
void BusinessService::InitUser(char *id, SOCKET sock) {

	char msg[BUF_SIZE];
	char name[NAME_SIZE];

	strncpy(name, idMap.find(id)->second.nickname, NAME_SIZE);

	LPPER_HANDLE_DATA handleInfo = new PER_HANDLE_DATA;
	cout << "START user : " << name << endl;
	cout << "sock : " << sock << endl;
	// 유저의 상태 정보 초기화
	// ioInfo = new PER_IO_DATA;
	handleInfo->status = STATUS_WAITING;

	strncpy(handleInfo->userId, id, NAME_SIZE);
	strncpy(handleInfo->userName, name, NAME_SIZE);
	strncpy(handleInfo->roomName, "", NAME_SIZE);
	EnterCriticalSection(&cs);
	// 세션정보 insert
	this->userMap.insert(pair<SOCKET, LPPER_HANDLE_DATA>(sock, handleInfo));
	cout << "현재 접속 인원 수 : " << userMap.size() << endl;
	LeaveCriticalSection(&cs);

	// id의 key값은 유저 이름이 아니라 아이디!!
	idMap.find(id)->second.logMode = NOW_LOGIN;

	strncpy(msg, "입장을 환영합니다!\n", BUF_SIZE);
	strcat(msg, waitRoomMessage);

	//LPPER_IO_DATA ioInfo = new PER_IO_DATA;
	LPPER_IO_DATA ioInfo = mp->malloc();
	iocpService->SendToOneMsg(msg, sock, STATUS_WAITING, ioInfo);
}

// 접속 강제종료 로직
void BusinessService::ClientExit(SOCKET sock) {

	if (closesocket(sock) != SOCKET_ERROR) {
		cout << "정상 종료 " << endl;

		if (userMap.find(sock) != userMap.end()) {

			// 세션정보 있을때는 => 세션정보 방 정보 제거 필요
			char roomName[NAME_SIZE];
			char name[NAME_SIZE];
			char id[NAME_SIZE];

			strncpy(roomName, userMap.find(sock)->second->roomName,
			NAME_SIZE);
			strncpy(name, userMap.find(sock)->second->userName, NAME_SIZE);
			strncpy(id, userMap.find(sock)->second->userId, NAME_SIZE);
			// 로그인 상태 원상복구
			idMap.find(id)->second.logMode = NOW_LOGOUT;

			// 방이름 임시 저장
			if (strcmp(roomName, "") != 0) { // 방에 접속중인 경우
				// 나가는 사람 정보 out

				if (roomMap.find(roomName) != roomMap.end()) { // null 체크 우선
					(roomMap.find(roomName)->second).remove(sock);

					if ((roomMap.find(roomName)->second).size() == 0) { // 방인원 0명이면 방 삭제
						EnterCriticalSection(&cs);
						roomMap.erase(roomName);
						LeaveCriticalSection(&cs);
					} else {
						char sendMsg[BUF_SIZE];
						// 이름먼저 복사 NAME_SIZE까지
						strncpy(sendMsg, name, NAME_SIZE);
						strcat(sendMsg, " 님이 나갔습니다!");

						if (roomMap.find(userMap.find(sock)->second->roomName)
								!= roomMap.end()) { // null 검사
								//LPPER_IO_DATA ioInfo = new PER_IO_DATA;
							LPPER_IO_DATA ioInfo = mp->malloc();
							iocpService->SendToRoomMsg(sendMsg,
									roomMap.find(
											userMap.find(sock)->second->roomName)->second,
									STATUS_CHATTIG, ioInfo, this->cs);
						}
					}
				}
			}
			EnterCriticalSection(&cs);
			userMap.erase(sock); // 접속 소켓 정보 삭제
			LeaveCriticalSection(&cs);
			cout << "현재 접속 인원 수 : " << userMap.size() << endl;
		}

	}
}

// 로그인 이전 로직처리
// 세션값 없을 때 로직
void BusinessService::StatusLogout(SOCKET &sock, int &status, int &direction,
		char *message) {

	if (status == STATUS_LOGOUT) {

		// memcpy 부분 삭제
		string msg = "";

		if (direction == USER_MAKE) { // 1번 계정생성
			cout << "USER MAKE" << endl;
			USER_DATA userData;
			char *sArr[3] = { NULL, };
			char *ptr = strtok(message, "\\"); // 공백 문자열을 기준으로 문자열을 자름
			int i = 0;
			while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
			{
				sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
				i++;                       // 인덱스 증가
				ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
			}
			// 유효성 검증 필요
			EnterCriticalSection(&cs); // 동시적으로 같은 ID 입력 방지
			if (idMap.find(sArr[0]) == idMap.end()) { // ID 중복체크

				userData.password = string(sArr[1]);
				strncpy(userData.nickname, sArr[2], NAME_SIZE);
				userData.logMode = NOW_LOGOUT;

				idMap[string(sArr[0])] = userData; // Insert

				LeaveCriticalSection(&cs); // Insert 후 Lock 해제
				msg.append(sArr[0]);
				msg.append(" 계정 생성 완료!\n");
				msg.append(loginBeforeMessage);
				LPPER_IO_DATA ioInfo = mp->malloc();
				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT,
						ioInfo);
			} else { // ID중복있음
				LeaveCriticalSection(&cs); // Case Lock 해제
				msg.append("중복된 아이디가 있습니다!\n");
				msg.append(loginBeforeMessage);
				LPPER_IO_DATA ioInfo = mp->malloc();
				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT,
						ioInfo);
			}

		} else if (direction == USER_ENTER) { // 2번 로그인 시도
			cout << "USER ENTER" << endl;
			char *sArr[2] = { NULL, };
			char *ptr = strtok(message, "\\"); // 공백 문자열을 기준으로 문자열을 자름
			int i = 0;
			while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
			{
				sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
				i++;                       // 인덱스 증가
				ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
			}

			if (idMap.find(sArr[0]) == idMap.end()) { // 계정 없음
				msg.append("없는 계정입니다 아이디를 확인하세요!\n");
				msg.append(loginBeforeMessage);
				LPPER_IO_DATA ioInfo = mp->malloc();
				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT,
						ioInfo);
			} else if (idMap.find(sArr[0])->second.password.compare(
					string(sArr[1])) == 0) { // 비밀번호 일치

				if (idMap.find(sArr[0])->second.logMode == NOW_LOGIN) { // 중복로그인 유효성 검사
					msg.append("중복 로그인은 안됩니다!\n");
					msg.append(loginBeforeMessage);
					LPPER_IO_DATA ioInfo = mp->malloc();
					iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT,
							ioInfo);
				} else { // 중복로그인 X
					InitUser(sArr[0], sock); // 세션정보 저장
				}

			} else { // 비밀번호 틀림
				msg.append("비밀번호 틀림!\n");
				msg.append(loginBeforeMessage);
				LPPER_IO_DATA ioInfo = mp->malloc();
				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT,
						ioInfo);
			}

		} else if (direction == USER_LIST) { // 유저들 정보 요청
			cout << "USER LIST" << endl;
			char userList[BUF_SIZE];
			strncpy(userList, "아이디 정보 리스트", BUF_SIZE);
			if (idMap.size() == 0) {
				strcat(userList, " 없음");
			} else {
				// 변경 아니므로 Lock 없이
				unordered_map<string, USER_DATA>::const_iterator iter;
				for (iter = idMap.begin(); iter != idMap.end(); ++iter) {
					strcat(userList, "\n");
					strcat(userList, (iter->first).c_str());
				}
			}
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToOneMsg(userList, sock, STATUS_LOGOUT, ioInfo);
		} else { // 그외 명령어 입력
			char sendMsg[BUF_SIZE];

			strncpy(sendMsg, errorMessage, BUF_SIZE);
			strcat(sendMsg, loginBeforeMessage);
			// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToOneMsg(sendMsg, sock, STATUS_LOGOUT, ioInfo);
		}
	}

}

// 대기실에서의 로직 처리
// 세션값 있음
void BusinessService::StatusWait(SOCKET &sock, int &status, int &direction,
		char *message) {

	string name = userMap.find(sock)->second->userName;
	string msg = string(message);
	// 세션에서 이름 정보 리턴

	if (direction == ROOM_MAKE) { // 새로운 방 만들때
		cout << "ROOM MAKE" << endl;
		// 유효성 검증 먼저
		if (roomMap.count(msg) != 0) { // 방이름 중복
			msg += "이미 있는 방 이름입니다!\n";
			msg += waitRoomMessage;
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_WAITING,
					ioInfo);
			// 중복 케이스는 방 만들 수 없음
		} else { // 방이름 중복 아닐 때만 개설

			// 새로운 방 정보 저장
			list<SOCKET> chatList;
			chatList.push_back(sock);
			EnterCriticalSection(&cs);
			roomMap.insert(pair<string, list<SOCKET> >(msg, chatList));
			LeaveCriticalSection(&cs);

			// User의 상태 정보 바꾼다
			strncpy((userMap.find(sock))->second->roomName, msg.c_str(),
			NAME_SIZE);
			(userMap.find(sock))->second->status =
			STATUS_CHATTIG;

			msg += " 방이 개설되었습니다.";
			msg += chatRoomMessage;
			cout << "현재 서버의 방 갯수 : " << roomMap.size() << endl;
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_CHATTIG,
					ioInfo);
		}

	} else if (direction == ROOM_ENTER) { // 방 입장 요청
		cout << "ROOM ENTER" << endl;

		// 유효성 검증 먼저
		if (roomMap.count(msg) == 0) { // 방 못찾음
			msg = "없는 방 입니다!\n";
			msg += waitRoomMessage;
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_WAITING,
					ioInfo);
		} else {
			EnterCriticalSection(&cs);

			if (roomMap.find(msg) != roomMap.end()) {
				roomMap.find(msg)->second.push_back(sock);
			}

			strncpy(userMap.find(sock)->second->roomName, msg.c_str(),
			NAME_SIZE);
			userMap.find(sock)->second->status = STATUS_CHATTIG;

			LeaveCriticalSection(&cs);

			string sendMsg = name;
			sendMsg += " 님이 입장하셨습니다. ";
			sendMsg += chatRoomMessage;

			if (roomMap.find(msg) != roomMap.end()) { // 안전성 검사
				LPPER_IO_DATA ioInfo = mp->malloc();
				iocpService->SendToRoomMsg(sendMsg.c_str(),
						roomMap.find(msg)->second,
						STATUS_CHATTIG, ioInfo, this->cs);

			}
		}

	} else if (direction == ROOM_INFO) { // 방 정보 요청시

		char str[BUF_SIZE];
		if (roomMap.size() == 0) {
			strncpy(str, "만들어진 방이 없습니다", BUF_SIZE);
		} else {
			strncpy(str, "방 정보 리스트", BUF_SIZE);

			unordered_map<string, list<SOCKET> >::const_iterator iter;
			// 방정보를 문자열로 만든다
			EnterCriticalSection(&cs);
			for (iter = roomMap.begin(); iter != roomMap.end(); iter++) {
				char num[4];
				strcat(str, "\n");
				strcat(str, iter->first.c_str());
				strcat(str, " : ");
				sprintf(num, "%d", (iter->second).size());
				strcat(str, num);
				strcat(str, "명");
			}
			LeaveCriticalSection(&cs);
		}
		LPPER_IO_DATA ioInfo = mp->malloc();
		iocpService->SendToOneMsg(str, sock, STATUS_WAITING, ioInfo);
	} else if (direction == ROOM_USER_INFO) { // 유저 정보 요청시
		char str[BUF_SIZE];

		unordered_map<SOCKET, LPPER_HANDLE_DATA>::iterator iter;
		strncpy(str, "유저 정보 리스트", BUF_SIZE);
		// 유저 정보를 문자열로 만든다
		EnterCriticalSection(&cs);
		for (iter = userMap.begin(); iter != userMap.end(); iter++) {
			strcat(str, "\n");
			strcat(str, (iter->second)->userName);
			strcat(str, " : ");
			if ((iter->second)->status == STATUS_WAITING) {
				strcat(str, "대기실 ");
			} else {
				strcat(str, (iter->second)->roomName);
			}
		}
		LeaveCriticalSection(&cs);
		LPPER_IO_DATA ioInfo = mp->malloc();
		iocpService->SendToOneMsg(str, sock, STATUS_WAITING, ioInfo);
	} else if (direction == WHISPER) { // 귓속말
		cout << "WHISPER " << endl;
		char *sArr[2] = { NULL, };
		char *ptr = strtok(message, "\\"); // 공백 문자열을 기준으로 문자열을 자름
		int i = 0;
		while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		name = string(sArr[0]);
		msg = string(sArr[1]);

		unordered_map<SOCKET, LPPER_HANDLE_DATA>::iterator iter;
		string sendMsg;

		if (name.compare(userMap.find(sock)->second->userName) == 0) { // 본인에게 쪽지
			sendMsg = "자신에게 쪽지를 보낼수 없습니다\n";
			sendMsg += waitRoomMessage;
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToOneMsg(sendMsg.c_str(), sock, STATUS_WAITING,
					ioInfo);
		} else {
			bool find = false;
			EnterCriticalSection(&cs);
			for (iter = userMap.begin(); iter != userMap.end(); iter++) {
				if (name.compare(iter->second->userName) == 0) {
					find = true;
					sendMsg = userMap.find(sock)->second->userName;
					sendMsg += " 님에게 온 귓속말 : ";
					sendMsg += msg;
					LPPER_IO_DATA ioInfo = mp->malloc();
					iocpService->SendToOneMsg(sendMsg.c_str(), iter->first,
					STATUS_WHISPER, ioInfo);
					break;
				}
			}
			LeaveCriticalSection(&cs);
			// 귓속말 대상자 못찾음
			if (!find) {
				sendMsg = name;
				sendMsg += " 님을 찾을 수 없습니다";
				LPPER_IO_DATA ioInfo = mp->malloc();
				iocpService->SendToOneMsg(sendMsg.c_str(), sock, STATUS_WAITING,
						ioInfo);
			}
		}

	} else if (msg.compare("6") == 0) { // 로그아웃
		EnterCriticalSection(&cs);

		string userId = idMap.find(userMap.find(sock)->second->userId)->first;
		userMap.erase(sock); // 접속 소켓 정보 삭제
		idMap.find(userId)->second.logMode = NOW_LOGOUT;
		LeaveCriticalSection(&cs);
		char sendMsg[BUF_SIZE];

		strncpy(sendMsg, loginBeforeMessage, BUF_SIZE);
		LPPER_IO_DATA ioInfo = mp->malloc();
		iocpService->SendToOneMsg(sendMsg, sock, STATUS_LOGOUT, ioInfo);
	} else { // 그외 명령어 입력
		char sendMsg[BUF_SIZE];

		strncpy(sendMsg, errorMessage, BUF_SIZE);
		strcat(sendMsg, waitRoomMessage);
		// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다
		LPPER_IO_DATA ioInfo = mp->malloc();
		iocpService->SendToOneMsg(sendMsg, sock, STATUS_WAITING, ioInfo);
	}
}

// 채팅방에서의 로직 처리
// 세션값 있음
void BusinessService::StatusChat(SOCKET &sock, int &status, int &direction,
		char *message) {

	string name;
	string msg;

	// 세션에서 이름 정보 리턴
	name = userMap.find(sock)->second->userName;
	msg = message;

	if (msg.compare("\\out") == 0) { // 채팅방 나감

		string sendMsg;
		string roomName;
		sendMsg = name;
		sendMsg += " 님이 나갔습니다!";

		if (roomMap.find(userMap.find(sock)->second->roomName)
				!= roomMap.end()) { // null 검사
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToRoomMsg(sendMsg.c_str(),
					roomMap.find(userMap.find(sock)->second->roomName)->second,
					STATUS_CHATTIG, ioInfo, this->cs);

			roomName = userMap.find(sock)->second->roomName;
			// 방이름 임시 저장
			strncpy(userMap.find(sock)->second->roomName, "", NAME_SIZE);
			userMap.find(sock)->second->status = STATUS_WAITING;
		}

		msg = waitRoomMessage;
		LPPER_IO_DATA ioInfo = mp->malloc();
		iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_WAITING, ioInfo);
		cout << "나가는 사람 name : " << name << endl;
		cout << "나가는 방 name : " << roomName << endl;

		// 나가는 사람 정보 out
		EnterCriticalSection(&cs);
		(roomMap.find(roomName)->second).remove(sock);

		if ((roomMap.find(roomName)->second).size() == 0) { // 방인원 0명이면 방 삭제
			roomMap.erase(roomName);
		}
		LeaveCriticalSection(&cs);
	} else { // 채팅방에서 채팅중

		string sendMsg;
		sendMsg = name;
		sendMsg += " : ";
		sendMsg += msg;

		if (roomMap.find(userMap.find(sock)->second->roomName)
				!= roomMap.end()) { // null 검사
			LPPER_IO_DATA ioInfo = mp->malloc();
			iocpService->SendToRoomMsg(sendMsg.c_str(),
					roomMap.find(userMap.find(sock)->second->roomName)->second,
					STATUS_CHATTIG, ioInfo, this->cs);
		}
	}

}

// 클라이언트에게 받은 데이터 복사후 구조체 해제
char* BusinessService::DataCopy(LPPER_IO_DATA ioInfo, int *status,
		int *direction) {
	memcpy(status, ((char*) ioInfo->recvBuffer) + 4, 4); // Status
	memcpy(direction, ((char*) ioInfo->recvBuffer) + 8, 4); // Direction
	char* msg = new char[ioInfo->bodySize];	// Msg
	memcpy(msg, ((char*) ioInfo->recvBuffer) + 12, ioInfo->bodySize);
	// 다 복사 받았으니 할당 해제
	delete ioInfo->recvBuffer;
	// 메모리 해제
	// delete ioInfo;
	mp->free(ioInfo);

	return msg;
}

// 패킷 데이터 읽기
void BusinessService::PacketReading(LPPER_IO_DATA ioInfo, DWORD bytesTrans) {

	// IO 완료후 동작 부분
	if (READ == ioInfo->serverMode) {
		if (bytesTrans < 4) { // 헤더를 다 못 읽어온 상황
			memcpy(((char*) &(ioInfo->bodySize)) + ioInfo->recvByte,
					ioInfo->buffer, bytesTrans);
		} else {
			memcpy(&(ioInfo->bodySize), ioInfo->buffer, 4);
			ioInfo->recvBuffer = new char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당
			memcpy(((char*) ioInfo->recvBuffer), ioInfo->buffer, bytesTrans);
		}
		ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
	} else { // 더 읽기
		if (ioInfo->recvByte >= 4) { // 헤더 다 읽었음
			memcpy(((char*) ioInfo->recvBuffer) + ioInfo->recvByte,
					ioInfo->buffer, bytesTrans);
			ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
		} else { // 헤더를 다 못읽었음 경우
			int recv = min(4 - ioInfo->recvByte, bytesTrans);
			memcpy(((char*) &(ioInfo->bodySize)) + ioInfo->recvByte,
					ioInfo->buffer, recv); // 헤더부터 채운다
			ioInfo->recvByte += bytesTrans; // 지금까지 받은 데이터 수 갱신
			if (ioInfo->recvByte >= 4) {
				ioInfo->recvBuffer = new char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당
				memcpy(((char*) ioInfo->recvBuffer) + 4,
						((char*) ioInfo->buffer) + recv, bytesTrans - recv); // 이제 헤더는 필요없음 => 이때는 뒤의 데이터만 읽자
			}
		}
	}

	if (ioInfo->totByte == 0 && ioInfo->recvByte >= 4) { // 헤더를 다 읽어야 토탈 바이트 수를 알 수 있다
		ioInfo->totByte = ioInfo->bodySize + 12;
	}
}

LPPER_IO_DATA BusinessService::malloc() {
	return mp->malloc();
}

void BusinessService::free(LPPER_IO_DATA freePoint) {
	mp->free(freePoint);
}

IocpService::IocpService* BusinessService::getIocpService() {
	return this->iocpService;
}

} /* namespace Service */
