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
	InitializeCriticalSectionAndSpinCount(&idCs, 2000);

	InitializeCriticalSectionAndSpinCount(&userCs, 2000);

	InitializeCriticalSectionAndSpinCount(&roomCs, 2000);

	iocpService = new IocpService::IocpService();
}

BusinessService::~BusinessService() {

	delete iocpService;
	// 임계영역 Object 반환
	DeleteCriticalSection(&idCs);

	DeleteCriticalSection(&userCs);

	DeleteCriticalSection(&roomCs);
}

// 초기 로그인
// 세션정보 추가
void BusinessService::InitUser(const char *id, SOCKET sock) {

	string msg;
	char name[NAME_SIZE];

	strncpy(name, idMap.find(id)->second.nickname, NAME_SIZE);

	PER_HANDLE_DATA userInfo;
	cout << "START user : " << name << endl;
	// cout << "sock : " << sock << endl;
	// 유저의 상태 정보 초기화
	userInfo.status = STATUS_WAITING;

	strncpy(userInfo.userId, id, NAME_SIZE);
	strncpy(userInfo.userName, name, NAME_SIZE);
	strncpy(userInfo.roomName, "", NAME_SIZE);

	EnterCriticalSection(&userCs);
	// 세션정보 insert
	this->userMap[sock] = userInfo;
	cout << "현재 접속 인원 수 : " << userMap.size() << endl;
	LeaveCriticalSection(&userCs);

	// id의 key값은 유저 이름이 아니라 아이디!!
	idMap.find(id)->second.logMode = NOW_LOGIN;

	msg = "입장을 환영합니다!\n";
	msg.append(waitRoomMessage);

	iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_WAITING);
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

			strncpy(roomName, userMap.find(sock)->second.roomName,
			NAME_SIZE);
			strncpy(name, userMap.find(sock)->second.userName, NAME_SIZE);
			strncpy(id, userMap.find(sock)->second.userId, NAME_SIZE);
			// 로그인 상태 원상복구
			idMap.find(id)->second.logMode = NOW_LOGOUT;

			// 방이름 임시 저장
			if (strcmp(roomName, "") != 0) { // 방에 접속중인 경우
				// 나가는 사람 정보 out

				if (roomMap.find(roomName) != roomMap.end()) { // null 체크 우선
					EnterCriticalSection(
							&roomMap.find(roomName)->second.listCs);
					(roomMap.find(roomName)->second).userList.remove(sock);
					LeaveCriticalSection(
							&roomMap.find(roomName)->second.listCs);

					if ((roomMap.find(roomName)->second).userList.size() == 0) { // 방인원 0명이면 방 삭제
						DeleteCriticalSection(
								&roomMap.find(roomName)->second.listCs);
						// 방 별로 가진 CS를 Delete 친다
						EnterCriticalSection(&roomCs);
						roomMap.erase(roomName);
						LeaveCriticalSection(&roomCs);
					} else {
						char sendMsg[BUF_SIZE];
						// 이름먼저 복사 NAME_SIZE까지
						strncpy(sendMsg, name, NAME_SIZE);
						strcat(sendMsg, " 님이 나갔습니다!");

						if (roomMap.find(userMap.find(sock)->second.roomName)
								!= roomMap.end()) { // null 검사

							iocpService->SendToRoomMsg(sendMsg,
									roomMap.find(
											userMap.find(sock)->second.roomName)->second.userList,
									STATUS_CHATTIG,
									&roomMap.find(
											userMap.find(sock)->second.roomName)->second.listCs);
						}
					}
				}
			}
			EnterCriticalSection(&userCs);
			userMap.erase(sock); // 접속 소켓 정보 삭제
			LeaveCriticalSection(&userCs);
			cout << "현재 접속 인원 수 : " << userMap.size() << endl;
		}

	}
}

// 로그인 이전 로직처리
// 세션값 없을 때 로직
void BusinessService::StatusLogout(SOCKET sock, int status, int direction,
		char *message) {

	if (status == STATUS_LOGOUT) {

		// memcpy 부분 삭제
		string msg = "";

		if (direction == USER_MAKE) { // 1번 계정생성
			// cout << "USER MAKE" << endl;
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
			EnterCriticalSection(&idCs); // 동시적으로 같은 ID 입력 방지
			if (idMap.find(sArr[0]) == idMap.end()) { // ID 중복체크

				userData.password = string(sArr[1]);
				strncpy(userData.nickname, sArr[2], NAME_SIZE);
				userData.logMode = NOW_LOGOUT;

				idMap[string(sArr[0])] = userData; // Insert

				LeaveCriticalSection(&idCs); // Insert 후 Lock 해제
				msg.append(sArr[0]);
				msg.append(" 계정 생성 완료!\n");
				msg.append(loginBeforeMessage);

				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT);
			} else { // ID중복있음
				LeaveCriticalSection(&idCs); // Case Lock 해제
				msg.append("중복된 아이디가 있습니다!\n");
				msg.append(loginBeforeMessage);

				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT);
			}

		} else if (direction == USER_ENTER) { // 2번 로그인 시도
			// cout << "USER ENTER" << endl;
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

				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT);
			} else if (idMap.find(sArr[0])->second.password.compare(
					string(sArr[1])) == 0) { // 비밀번호 일치

				if (idMap.find(sArr[0])->second.logMode == NOW_LOGIN) { // 중복로그인 유효성 검사
					msg.append("중복 로그인은 안됩니다!\n");
					msg.append(loginBeforeMessage);

					iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT);
				} else { // 중복로그인 X
					InitUser(sArr[0], sock); // 세션정보 저장
				}

			} else { // 비밀번호 틀림
				msg.append("비밀번호 틀림!\n");
				msg.append(loginBeforeMessage);

				iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_LOGOUT);
			}

		} else if (direction == USER_LIST) { // 유저들 정보 요청
			// cout << "USER LIST" << endl;
			string userList = "아이디 정보 리스트";
			if (idMap.size() == 0) {
				userList += " 없음";
			} else {
				unordered_map<string, USER_DATA> idCopyMap = idMap;
				// 동기화 없이 깊은복사
				unordered_map<string, USER_DATA>::const_iterator iter;
				for (iter = idCopyMap.begin(); iter != idCopyMap.end();
						++iter) {
					userList += "\n";
					userList += (iter->first).c_str();
				}
			}
			iocpService->SendToOneMsg(userList.c_str(), sock, STATUS_LOGOUT);
		} else { // 그외 명령어 입력
			string sendMsg = errorMessage;
			// sendMsg += waitRoomMessage;
			// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다
			iocpService->SendToOneMsg(sendMsg.c_str(), sock, STATUS_LOGOUT);
		}
	}
	CharPool* charPool = CharPool::getInstance();
	charPool->free(message);
}

// 대기실에서의 로직 처리
// 세션값 있음
void BusinessService::StatusWait(SOCKET sock, int status, int direction,
		char *message) {

	string name = userMap.find(sock)->second.userName;
	string msg = string(message);
	// 세션에서 이름 정보 리턴

	if (direction == ROOM_MAKE) { // 새로운 방 만들때
		// cout << "ROOM MAKE" << endl;
		// 유효성 검증 먼저
		if (roomMap.count(msg) != 0) { // 방이름 중복
			msg += "이미 있는 방 이름입니다!\n";
			msg += waitRoomMessage;

			iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_WAITING);
			// 중복 케이스는 방 만들 수 없음
		} else { // 방이름 중복 아닐 때만 개설

			// 새로운 방 정보 저장
			ROOM_DATA roomData;
			list<SOCKET> chatList;
			chatList.push_back(sock);
			// 방 리스트별 CS객체
			CRITICAL_SECTION cs;
			InitializeCriticalSection(&cs);
			// 방 리스트별 CS객체 Init
			roomData.userList = chatList;
			roomData.listCs = cs;

			EnterCriticalSection(&roomCs);
			roomMap[msg] = roomData;
			LeaveCriticalSection(&roomCs);

			// User의 상태 정보 바꾼다
			strncpy((userMap.find(sock))->second.roomName, msg.c_str(),
			NAME_SIZE);
			(userMap.find(sock))->second.status =
			STATUS_CHATTIG;

			msg += " 방이 개설되었습니다.";
			msg += chatRoomMessage;
			// cout << "현재 서버의 방 갯수 : " << roomMap.size() << endl;

			iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_CHATTIG);
		}

	} else if (direction == ROOM_ENTER) { // 방 입장 요청
		// cout << "ROOM ENTER" << endl;

		// 유효성 검증 먼저
		EnterCriticalSection(&roomCs);
		if (roomMap.count(msg) == 0) { // 방 못찾음
			LeaveCriticalSection(&roomCs);
			msg = "없는 방 입니다!\n";
			msg += waitRoomMessage;
			;
			iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_WAITING);
		} else {
			LeaveCriticalSection(&roomCs); // 방 삭제와 방 입장 동시 케이스 생각

			//방이 있으니까 유저를 insert

			EnterCriticalSection(&(roomMap.find(msg)->second.listCs));
			roomMap.find(msg)->second.userList.push_back(sock);
			LeaveCriticalSection(&(roomMap.find(msg)->second.listCs));

			strncpy(userMap.find(sock)->second.roomName, msg.c_str(),
			NAME_SIZE);
			userMap.find(sock)->second.status = STATUS_CHATTIG;

			string sendMsg = name;
			sendMsg += " 님이 입장하셨습니다. ";
			sendMsg += chatRoomMessage;

			if (roomMap.find(msg) != roomMap.end()) { // 안전성 검사

				iocpService->SendToRoomMsg(sendMsg.c_str(),
						roomMap.find(msg)->second.userList,
						STATUS_CHATTIG, &roomMap.find(msg)->second.listCs);

			}
		}

	} else if (direction == ROOM_INFO) { // 방 정보 요청시

		string str;
		if (roomMap.size() == 0) {
			str = "만들어진 방이 없습니다";
		} else {
			str += "방 정보 리스트";

			unordered_map<string, ROOM_DATA> roomCopyMap = roomMap;
			// 동기화 없이 깊은복사
			unordered_map<string, ROOM_DATA>::const_iterator iter;
			// 방정보를 문자열로 만든다

			for (iter = roomCopyMap.begin(); iter != roomCopyMap.end();
					iter++) {

				str += "\n";
				str += iter->first.c_str();
				str += " : ";
				//str += to_string((iter->second).userList.size());
				str += "명";
			}

		}

		iocpService->SendToOneMsg(str.c_str(), sock, STATUS_WAITING);
	} else if (direction == ROOM_USER_INFO) { // 유저 정보 요청시
		string str = "유저 정보 리스트";

		unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap = userMap;
		// 동기화 없이 깊은복사
		unordered_map<SOCKET, PER_HANDLE_DATA>::iterator iter;

		for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
			str += "\n";
			str += (iter->second).userName;
			str += " : ";
			if ((iter->second).status == STATUS_WAITING) {
				str += "대기실 ";
			} else {
				str += (iter->second).roomName;
			}
		}

		iocpService->SendToOneMsg(str.c_str(), sock, STATUS_WAITING);
	} else if (direction == WHISPER) { // 귓속말
		// cout << "WHISPER " << endl;
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

		string sendMsg;

		if (name.compare(userMap.find(sock)->second.userName) == 0) { // 본인에게 쪽지
			sendMsg = "자신에게 쪽지를 보낼수 없습니다\n";
			sendMsg += waitRoomMessage;

			iocpService->SendToOneMsg(sendMsg.c_str(), sock, STATUS_WAITING);
		} else {
			bool find = false;
			unordered_map<SOCKET, PER_HANDLE_DATA>::iterator iter;
			unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap = userMap;
			// 동기화 없이 깊은복사

			for (iter = userCopyMap.begin(); iter != userCopyMap.end();
					iter++) {
				if (name.compare(iter->second.userName) == 0) {
					find = true;
					sendMsg = userMap.find(sock)->second.userName;
					sendMsg += " 님에게 온 귓속말 : ";
					sendMsg += msg;

					iocpService->SendToOneMsg(sendMsg.c_str(), iter->first,
					STATUS_WHISPER);
					break;
				}
			}

			// 귓속말 대상자 못찾음
			if (!find) {
				sendMsg = name;
				sendMsg += " 님을 찾을 수 없습니다";

				iocpService->SendToOneMsg(sendMsg.c_str(), sock,
				STATUS_WAITING);
			}
		}

	} else if (msg.compare("6") == 0) { // 로그아웃

		string userId = idMap.find(userMap.find(sock)->second.userId)->first;
		idMap.find(userId)->second.logMode = NOW_LOGOUT;

		EnterCriticalSection(&userCs);
		userMap.erase(sock); // 접속 소켓 정보 삭제
		LeaveCriticalSection(&userCs);
		string sendMsg = loginBeforeMessage;

		iocpService->SendToOneMsg(sendMsg.c_str(), sock, STATUS_LOGOUT);
	} else { // 그외 명령어 입력
		string sendMsg = errorMessage;
		// sendMsg += waitRoomMessage;
		// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다

		iocpService->SendToOneMsg(sendMsg.c_str(), sock, STATUS_WAITING);
	}

	CharPool* charPool = CharPool::getInstance();
	charPool->free(message);
}

// 채팅방에서의 로직 처리
// 세션값 있음
void BusinessService::StatusChat(SOCKET sock, int status, int direction,
		char *message) {

	string name;
	string msg;

	// 세션에서 이름 정보 리턴
	name = userMap.find(sock)->second.userName;
	msg = string(message);

	if (msg.compare("\\out") == 0) { // 채팅방 나감

		string sendMsg;
		string roomName;
		sendMsg = name;
		sendMsg += " 님이 나갔습니다!";

		if (roomMap.find(userMap.find(sock)->second.roomName)
				!= roomMap.end()) { // null 검사

			iocpService->SendToRoomMsg(sendMsg.c_str(),
					roomMap.find(userMap.find(sock)->second.roomName)->second.userList,
					STATUS_CHATTIG,
					&roomMap.find(userMap.find(sock)->second.roomName)->second.listCs);

			roomName = userMap.find(sock)->second.roomName;
			// 방이름 임시 저장
			strncpy(userMap.find(sock)->second.roomName, "", NAME_SIZE);
			userMap.find(sock)->second.status = STATUS_WAITING;
		}

		msg = waitRoomMessage;

		iocpService->SendToOneMsg(msg.c_str(), sock, STATUS_WAITING);
		// cout << "나가는 사람 name : " << name << endl;
		// cout << "나가는 방 name : " << roomName << endl;

		// 나가는 사람 정보 out
		EnterCriticalSection(&roomMap.find(roomName)->second.listCs);
		(roomMap.find(roomName)->second).userList.remove(sock);
		LeaveCriticalSection(&roomMap.find(roomName)->second.listCs);

		EnterCriticalSection(&roomCs);
		if ((roomMap.find(roomName)->second).userList.size() == 0) { // 방인원 0명이면 방 삭제
			DeleteCriticalSection(&roomMap.find(roomName)->second.listCs);
			roomMap.erase(roomName);
		}
		LeaveCriticalSection(&roomCs);

	} else { // 채팅방에서 채팅중

		string sendMsg;
		sendMsg = name;
		sendMsg += " : ";
		sendMsg += msg;

		if (roomMap.find(userMap.find(sock)->second.roomName)
				!= roomMap.end()) { // null 검사

			iocpService->SendToRoomMsg(sendMsg.c_str(),
					roomMap.find(userMap.find(sock)->second.roomName)->second.userList,
					STATUS_CHATTIG,
					&roomMap.find(userMap.find(sock)->second.roomName)->second.listCs);
		}
	}
	CharPool* charPool = CharPool::getInstance();
	charPool->free(message);
}

// 클라이언트에게 받은 데이터 복사후 구조체 해제
char* BusinessService::DataCopy(LPPER_IO_DATA ioInfo, int *status,
		int *direction) {
	memcpy(status, ((char*) ioInfo->recvBuffer) + 4, 4); // Status
	memcpy(direction, ((char*) ioInfo->recvBuffer) + 8, 4); // Direction
	CharPool* charPool = CharPool::getInstance();
	char* msg = charPool->malloc(); // char[ioInfo->bodySize];	// Msg

	memcpy(msg, ((char*) ioInfo->recvBuffer) + 12, ioInfo->bodySize);
	// 다 복사 받았으니 할당 해제
	charPool->free(ioInfo->recvBuffer);

	MPool* mp = MPool::getInstance();
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
			CharPool* charPool = CharPool::getInstance();
			ioInfo->recvBuffer = charPool->malloc(); // char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당

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
				CharPool* charPool = CharPool::getInstance();
				ioInfo->recvBuffer = charPool->malloc(); // char[ioInfo->bodySize + 12]; // BodySize만큼 동적 할당

				memcpy(((char*) ioInfo->recvBuffer) + 4,
						((char*) ioInfo->buffer) + recv, bytesTrans - recv); // 이제 헤더는 필요없음 => 이때는 뒤의 데이터만 읽자
			}
		}
	}

	if (ioInfo->totByte == 0 && ioInfo->recvByte >= 4) { // 헤더를 다 읽어야 토탈 바이트 수를 알 수 있다
		ioInfo->totByte = ioInfo->bodySize + 12;
	}
}

IocpService::IocpService* BusinessService::getIocpService() {
	return this->iocpService;
}

} /* namespace Service */
