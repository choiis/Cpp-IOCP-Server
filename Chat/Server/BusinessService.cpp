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

	InitializeCriticalSectionAndSpinCount(&sqlCs, 2000);

	InitializeCriticalSectionAndSpinCount(&sendCs, 2000);

	iocpService = new IocpService::IocpService();
	
	dao = new Dao();
}

BusinessService::~BusinessService() {

	delete dao;

	delete iocpService;
	// 임계영역 Object 반환
	DeleteCriticalSection(&idCs);

	DeleteCriticalSection(&userCs);

	DeleteCriticalSection(&roomCs);

	DeleteCriticalSection(&sqlCs);

	DeleteCriticalSection(&sendCs);
}

void BusinessService::SQLwork() {

	SQL_DATA sqlData;
	
	EnterCriticalSection(&sqlCs);
	
	if (sqlQueue.size() != 0) {
		sqlData = sqlQueue.front();
		sqlQueue.pop();
	
		switch (sqlData.direction)
		{
		case UPDATE_USER: // ID정보 update
			dao->UpdateUser(sqlData.vo);
			break;
		case INSERT_LOGIN: // 로그인 정보 insert
			dao->InsertLogin(sqlData.vo);
			break;
		case INSERT_DIRECTION: // 지시 로그 insert
			dao->InsertDirection(sqlData.vo);
			break;
		case INSERT_CHATTING: // 채팅 로그 insert
			dao->InsertChatting(sqlData.vo);
			break; 
		default:
			break;
		}
	}
	LeaveCriticalSection(&sqlCs);
}


void BusinessService::Sendwork() {
	
	Send_DATA sendData;

	if (sendQueue.size() != 0) {
		EnterCriticalSection(&sendCs);
		sendData = sendQueue.front();
		sendQueue.pop();
		LeaveCriticalSection(&sendCs);
		switch (sendData.direction)
		{
		case SEND_ME: // Send to One
			iocpService->SendToOneMsg(sendData.msg.c_str(), sendData.mySocket, sendData.status);
			break;
		case SEND_ROOM: // Send to Room
			// LockCount가 있을 때 => 방 리스트가 살아있을 때
			{
				EnterCriticalSection(&roomCs);
				auto iter = roomMap.find(sendData.roomName);
				shared_ptr<ROOM_DATA> second;
				if (iter != roomMap.end()) {
					second = iter->second;
				}
				LeaveCriticalSection(&roomCs);

				if (second->listCs.LockCount == -1) {
					EnterCriticalSection(&second->listCs);
					iocpService->SendToRoomMsg(sendData.msg.c_str(), second->userList, sendData.status);
					LeaveCriticalSection(&second->listCs);
				}
				break;
			}
		default:
			break;
		}
	}
	else {
		Sleep(1);
	}
	
}

// InsertSendQueue 공통화
void BusinessService::InsertSendQueue(int direction, string msg, string roomName, SOCKET socket, int status){

	EnterCriticalSection(&sendCs);
	// SendQueue에 Insert
	Send_DATA sendData;
	if (direction == SEND_ME) {
		sendData.direction = direction;
		sendData.msg = msg;
		sendData.mySocket = socket;
		sendData.status = status;
	}
	else { //  SEND_ROOM
		sendData.direction = direction;
		sendData.msg = msg;
		sendData.roomName = roomName;
		sendData.status = status;
	}
	
	sendQueue.push(sendData);
	LeaveCriticalSection(&sendCs);
}

// 초기 로그인
// 세션정보 추가
void BusinessService::InitUser(const char *id, SOCKET sock, const char* nickName) {

	string msg;

	PER_HANDLE_DATA userInfo;
	// 유저의 상태 정보 초기화
	userInfo.status = STATUS_WAITING;

	strncpy(userInfo.userId, id, NAME_SIZE);
	strncpy(userInfo.roomName, "", NAME_SIZE);
	strncpy(userInfo.userName, nickName, NAME_SIZE);

	EnterCriticalSection(&userCs);
	// 세션정보 insert
	this->userMap[sock] = userInfo;
	cout << "START user : " << nickName << endl;
	cout << "현재 접속 인원 수 : " << userMap.size() << endl;
	LeaveCriticalSection(&userCs);

	Vo vo; // DB SQL문에 필요한 Data
	vo.setUserId(userInfo.userId);
	vo.setNickName(userInfo.userName);

	EnterCriticalSection(&sqlCs);
	
	SQL_DATA sqlData1, sqlData2;
	sqlData1.vo = vo;
	sqlData1.direction = UPDATE_USER;
	sqlQueue.push(sqlData1); // 최근 로그인기록 업데이트
	sqlData2.vo = vo;
	sqlData2.direction = INSERT_LOGIN;
	sqlQueue.push(sqlData2); // 로그인 DB에 기록

	LeaveCriticalSection(&sqlCs);

	msg = "입장을 환영합니다!\n";
	msg.append(waitRoomMessage);

	InsertSendQueue(SEND_ME, msg, "", sock, STATUS_WAITING);
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
			// 로그인 Set에서 out
			EnterCriticalSection(&idCs);
			idSet.erase(id);
			LeaveCriticalSection(&idCs);

			// 방이름 임시 저장
			if (userMap.find(sock)->second.status == STATUS_CHATTIG) { // 방에 접속중인 경우
				// 나가는 사람 정보 out

				EnterCriticalSection(&roomCs);
				
				if (roomMap.find(roomName) != roomMap.end()) { // null 체크 우선

					EnterCriticalSection(
							&roomMap.find(roomName)->second->listCs);
					roomMap.find(roomName)->second->userList.remove(sock);

					LeaveCriticalSection(
						&roomMap.find(roomName)->second->listCs);

					if ((roomMap.find(roomName)->second)->userList.size() == 0) { // 방인원 0명이면 방 삭제
						DeleteCriticalSection(
							&roomMap.find(roomName)->second->listCs);
						// 방 별로 가진 CS를 Delete 친다
						
						roomMap.erase(roomName);
						LeaveCriticalSection(&roomCs);
					} else {
						string sendMsg = name;
						// 이름먼저 복사 NAME_SIZE까지
						sendMsg += " 님이 나갔습니다!";

						EnterCriticalSection(&userCs);
						string roomN = userMap.find(sock)->second.roomName;
						LeaveCriticalSection(&userCs);
						int roomMapCnt = roomMap.count(roomN);

						if (roomMapCnt != 0) { // null 검사
							// SendQueue에 Insert
							InsertSendQueue(SEND_ROOM, sendMsg, userMap.find(sock)->second.roomName, 0, STATUS_CHATTIG);
						}

						LeaveCriticalSection(&roomCs);
					}
				}
				else {
					LeaveCriticalSection(&roomCs);
				}
			}
			EnterCriticalSection(&userCs);
			userMap.erase(sock); // 접속 소켓 정보 삭제
			cout << "현재 접속 인원 수 : " << userMap.size() << endl;
			LeaveCriticalSection(&userCs);
			
		}

	}
}

// 로그인 이전 로직처리
// 세션값 없을 때 로직
void BusinessService::StatusLogout(SOCKET sock, int direction, const char *message) {

	string msg = "";

	if (direction == USER_MAKE) { // 1번 계정생성

		char *sArr[3] = { NULL, };
		char message2[BUF_SIZE];
		strncpy(message2, message, BUF_SIZE);
		char *ptr = strtok(message2, "\\"); // 공백 문자열을 기준으로 문자열을 자름
		int i = 0;
		while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		// 유효성 검증 필요
		if (sArr[0] != NULL && sArr[1] != NULL && sArr[2] != NULL) {
			
			Vo vo;
			vo.setUserId(sArr[0]);
			vo = dao->selectUser(vo);

			if (strcmp(vo.getUserId(), "") == 0) { // ID 중복체크 => 계정 없음

				vo.setUserId(sArr[0]);
				vo.setPassword(sArr[1]);
				vo.setNickName(sArr[2]);

				dao->InsertUser(vo); 

				msg.append(sArr[0]);
				msg.append("계정 생성 완료!\n");
				msg.append(loginBeforeMessage);

				InsertSendQueue(SEND_ME, msg, "", sock, STATUS_LOGOUT);

			} else { // ID중복있음

				msg.append("중복된 아이디가 있습니다!\n");
				msg.append(loginBeforeMessage);

				InsertSendQueue(SEND_ME, msg, "", sock, STATUS_LOGOUT);
			}
		}

	} else if (direction == USER_ENTER) { // 2번 로그인 시도

		char *sArr[2] = { NULL, };
		char message2[BUF_SIZE];
		strncpy(message2, message, BUF_SIZE);
		char *ptr = strtok(message2, "\\"); // 공백 문자열을 기준으로 문자열을 자름
		int i = 0;
		while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		if (sArr[0] != NULL && sArr[1] != NULL) {
			
			Vo vo;
			vo.setUserId(sArr[0]);
			vo = dao->selectUser(vo);

			if (strcmp(vo.getUserId() , "") == 0) { // 계정 없음

				msg.append("없는 계정입니다 아이디를 확인하세요!\n");
				msg.append(loginBeforeMessage);

				// SendQueue에 Insert
				InsertSendQueue(SEND_ME, msg, "", sock, STATUS_LOGOUT);

			} else if (	strcmp(vo.getPassword(), sArr[1]) == 0	) { // 비밀번호 일치
				EnterCriticalSection(&idCs);
				unordered_set<string>::const_iterator it = idSet.find(sArr[0]);
				if (it != idSet.end()) { // 중복로그인 유효성 검사
					LeaveCriticalSection(&idCs); // Case Lock 해제
					msg.append("중복 로그인은 안됩니다!\n");
					msg.append(loginBeforeMessage);

					// SendQueue에 Insert
					InsertSendQueue(SEND_ME, msg, "", sock, STATUS_LOGOUT);

				} else { // 중복로그인 X
					idSet.insert(sArr[0]);
					LeaveCriticalSection(&idCs); //Init부분 두번동작 방지
					InitUser(sArr[0], sock , vo.getNickName()); // 세션정보 저장
				}

			} else { // 비밀번호 틀림

				msg.append("비밀번호 틀림!\n");
				msg.append(loginBeforeMessage);

				InsertSendQueue(SEND_ME, msg, "", sock, STATUS_LOGOUT);

			}
		}

	} else { // 그외 명령어 입력
		string sendMsg = errorMessage;
		sendMsg += waitRoomMessage;
		InsertSendQueue(SEND_ME, sendMsg, "", sock, STATUS_LOGOUT);
	}

}

// 대기실에서의 로직 처리
// 세션값 있음
void BusinessService::StatusWait(SOCKET sock, int status, int direction,
	const char *message) {

	string name = userMap.find(sock)->second.userName;
	string msg = string(message);
	// 세션에서 이름 정보 리턴

	if (direction == ROOM_MAKE) { // 새로운 방 만들때

		// 유효성 검증 먼저
		EnterCriticalSection(&roomCs);
		int cnt = roomMap.count(msg);

		if (cnt != 0) { // 방이름 중복
			LeaveCriticalSection(&roomCs);
			msg += "이미 있는 방 이름입니다!\n";
			msg += waitRoomMessage;
			InsertSendQueue(SEND_ME, msg, "", sock, STATUS_WAITING);
			// 중복 케이스는 방 만들 수 없음
		} else { // 방이름 중복 아닐 때만 개설
				 // 새로운 방 정보 저장
			shared_ptr<ROOM_DATA> roomData = make_shared<ROOM_DATA>();
			list<SOCKET> chatList;
			chatList.push_back(sock);
			InitializeCriticalSection(&roomData->listCs);
			// 방 리스트별 CS객체 Init
			roomData->userList = chatList;
			roomMap[msg] = roomData;

			LeaveCriticalSection(&roomCs);

			// User의 상태 정보 바꾼다
			EnterCriticalSection(&userCs);
			strncpy((userMap.find(sock))->second.roomName, msg.c_str(),
			NAME_SIZE);
			(userMap.find(sock))->second.status =
			STATUS_CHATTIG;
			LeaveCriticalSection(&userCs);

			msg += " 방이 개설되었습니다.";
			msg += chatRoomMessage;
			// cout << "현재 서버의 방 갯수 : " << roomMap.size() << endl;
			InsertSendQueue(SEND_ME, msg, "", sock, STATUS_CHATTIG);

		}

	} else if (direction == ROOM_ENTER) { // 방 입장 요청

		// 유효성 검증 먼저
		EnterCriticalSection(&roomCs);

		if (roomMap.find(msg) == roomMap.end()) { // 방 못찾음
			LeaveCriticalSection(&roomCs);
			msg = "없는 방 입니다!\n";
			msg += waitRoomMessage;

			InsertSendQueue(SEND_ME, msg, "", sock, STATUS_WAITING);

		} else {
			// LeaveCriticalSection(&roomCs);

			EnterCriticalSection(&userCs);
			strncpy(userMap.find(sock)->second.roomName, msg.c_str(),
			NAME_SIZE);
			userMap.find(sock)->second.status = STATUS_CHATTIG;
			LeaveCriticalSection(&userCs);

			string sendMsg = name;
			sendMsg += " 님이 입장하셨습니다. ";
			sendMsg += chatRoomMessage;
			if (roomMap.find(msg) != roomMap.end()) {
				EnterCriticalSection(&roomMap.find(msg)->second->listCs); // 방의 Lock
				//방이 있으니까 유저를 insert
				roomMap.find(msg)->second->userList.push_back(sock);
				LeaveCriticalSection(&roomMap.find(msg)->second->listCs); // 방의 Lock
			}
			
			LeaveCriticalSection(&roomCs); 
			InsertSendQueue(SEND_ROOM, sendMsg, msg, 0, STATUS_CHATTIG);
		}

	} else if (direction == ROOM_INFO) { // 방 정보 요청시

		string str;
		if (roomMap.size() == 0) {
			str = "만들어진 방이 없습니다";
		} else {
			str += "방 정보 리스트";
			EnterCriticalSection(&roomCs);
			unordered_map<string, shared_ptr<ROOM_DATA>> roomCopyMap = roomMap;
			LeaveCriticalSection(&roomCs);
			// 동기화 범위 좁게 깊은복사
			unordered_map<string, shared_ptr<ROOM_DATA>>::const_iterator iter;

			// 방정보를 문자열로 만든다
			for (iter = roomCopyMap.begin(); iter != roomCopyMap.end();
					iter++) {

				str += "\n";
				str += iter->first.c_str();
				str += ":";
				str += to_string((iter->second)->userList.size());
				str += "명";
			}

		}

		InsertSendQueue(SEND_ME, str, "", sock, STATUS_WAITING);

	} else if (direction == ROOM_USER_INFO) { // 유저 정보 요청시
		string str = "유저 정보 리스트";

		EnterCriticalSection(&userCs);
		unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap = userMap;
		LeaveCriticalSection(&userCs);
		// 동기화 범위 좁게 깊은복사
		unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter;
		for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
			if (str.length() >= 1024) {
				break;
			}
			str += "\n";
			str += (iter->second).userName;
			str += ":";
			if ((iter->second).status == STATUS_WAITING) {
				str += "대기실";
			} else {
				str += (iter->second).roomName;
			}
		}
		InsertSendQueue(SEND_ME, str, "", sock, STATUS_WAITING);

	} else if (direction == WHISPER) { // 귓속말

		char *sArr[2] = { NULL, };
		char message2[BUF_SIZE];
		strncpy(message2, message, BUF_SIZE);
		char *ptr = strtok(message2, "\\"); // 공백 문자열을 기준으로 문자열을 자름
		int i = 0;
		while (ptr != NULL)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(NULL, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		if (sArr[0] != NULL && sArr[1] != NULL) {
			name = string(sArr[0]);
			msg = string(sArr[1]);

			string sendMsg;

			if (name.compare(userMap.find(sock)->second.userName) == 0) { // 본인에게 쪽지
				sendMsg = "자신에게 쪽지를 보낼수 없습니다\n";
				sendMsg += waitRoomMessage;

				InsertSendQueue(SEND_ME, sendMsg, "", sock, STATUS_WAITING);

			} else {
				bool find = false;
				unordered_map<SOCKET, PER_HANDLE_DATA>::iterator iter;

				EnterCriticalSection(&userCs);
				for (iter = userMap.begin(); iter != userMap.end(); iter++) {
					if (name.compare(iter->second.userName) == 0) {
						find = true;
						sendMsg = userMap.find(sock)->second.userName;
						sendMsg += " 님에게 온 귓속말 : ";
						sendMsg += msg;

						InsertSendQueue(SEND_ME, sendMsg, "", iter->first, STATUS_WAITING);

						break;
					}
				}
				LeaveCriticalSection(&userCs);

				// 귓속말 대상자 못찾음
				if (!find) {
					sendMsg = name;
					sendMsg += " 님을 찾을 수 없습니다";

					InsertSendQueue(SEND_ME, sendMsg, "", sock, STATUS_WAITING);
				}
			}
		}

	} else if (msg.compare("6") == 0) { // 로그아웃

		EnterCriticalSection(&idCs);
		idSet.erase(userMap.find(sock)->second.userId); // 로그인 셋에서 제외
		LeaveCriticalSection(&idCs);

		EnterCriticalSection(&userCs);
		userMap.erase(sock); // 접속 소켓 정보 삭제
		cout << "현재 접속 인원 수 : " << userMap.size() << endl;
		LeaveCriticalSection(&userCs);

		string sendMsg = loginBeforeMessage;
		InsertSendQueue(SEND_ME, sendMsg, "", sock, STATUS_LOGOUT);
	} else { // 그외 명령어 입력
		string sendMsg = errorMessage;
		sendMsg += waitRoomMessage;
		// 대기방의 오류이므로 STATUS_WAITING 상태로 전달한다

		InsertSendQueue(SEND_ME, sendMsg, "", sock, STATUS_WAITING);
	}

	Vo vo; // DB SQL문에 필요한 Data
	vo.setNickName(name.c_str());
	vo.setMsg(message);
	vo.setDirection(direction);
	vo.setStatus(status);

	EnterCriticalSection(&sqlCs);
	SQL_DATA sqlData;
	sqlData.vo = vo;
	sqlData.direction = INSERT_DIRECTION;
	sqlQueue.push(sqlData); // 지시 기록 insert
	LeaveCriticalSection(&sqlCs);
}

// 채팅방에서의 로직 처리
// 세션값 있음
void BusinessService::StatusChat(SOCKET sock, int status, int direction,
	const char *message) {

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

		// EnterCriticalSection(&roomCs);
		// 방이름 임시 저장
		roomName = string(userMap.find(sock)->second.roomName);

		EnterCriticalSection(&roomMap.find(roomName)->second->listCs);

		roomMap.find(roomName)->second->userList.remove(sock); // 나가는 사람 정보 out

		LeaveCriticalSection(&roomMap.find(roomName)->second->listCs);
		
		// LeaveCriticalSection(&roomCs);
		// SendQueue에 Insert

		InsertSendQueue(SEND_ROOM, sendMsg, userMap.find(sock)->second.roomName, 0, STATUS_CHATTIG);

		msg = waitRoomMessage;
		InsertSendQueue(SEND_ME, msg, "", sock, STATUS_WAITING);

		EnterCriticalSection(&userCs);
		strncpy(userMap.find(sock)->second.roomName, "", NAME_SIZE); // 방이름 초기화
		userMap.find(sock)->second.status = STATUS_WAITING; // 상태 변경
		LeaveCriticalSection(&userCs);
		
		EnterCriticalSection(&roomCs);
		if ((roomMap.find(roomName)->second)->userList.size() == 0) { // 방인원 0명이면 방 삭제
			DeleteCriticalSection(&roomMap.find(roomName)->second->listCs);
			roomMap.erase(roomName);
		}
		LeaveCriticalSection(&roomCs);
	} else { // 채팅방에서 채팅중

		string sendMsg;
		sendMsg = name;
		sendMsg += " : ";
		sendMsg += msg;

		char roomName[NAME_SIZE];

		EnterCriticalSection(&userCs);
		unordered_map<string, shared_ptr<ROOM_DATA>>::iterator it = roomMap.find(
				userMap.find(sock)->second.roomName);
		strncpy(roomName, userMap.find(sock)->second.roomName, NAME_SIZE);
		LeaveCriticalSection(&userCs);
	
		Vo vo; // DB SQL문에 필요한 Data
		vo.setNickName(name.c_str());
		vo.setRoomName(roomName);
		vo.setMsg(msg.c_str());
		vo.setDirection(0);
		vo.setStatus(0);

		EnterCriticalSection(&sqlCs);

		SQL_DATA sqlData;
		sqlData.vo = vo;
		sqlData.direction = INSERT_CHATTING;
		sqlQueue.push(sqlData); // 대화 기록 업데이트

		LeaveCriticalSection(&sqlCs);

		if (it != roomMap.end()) { // null 검사
			InsertSendQueue(SEND_ROOM, sendMsg, userMap.find(sock)->second.roomName, 0, STATUS_CHATTIG);
		}
	}

}

// 클라이언트에게 받은 데이터 복사후 구조체 해제
// 사용 메모리 전부 반환
string BusinessService::DataCopy(LPPER_IO_DATA ioInfo, int *status,
	int *direction) {
	
	copy(((char*)ioInfo->recvBuffer) + 2, ((char*)ioInfo->recvBuffer) + 6,
		(char*)status);
	copy(((char*)ioInfo->recvBuffer) + 6, ((char*)ioInfo->recvBuffer) + 10,
		(char*)direction);

	CharPool* charPool = CharPool::getInstance();
	char* msg = charPool->Malloc(); // 512 Byte까지 카피 가능
	copy(((char*)ioInfo->recvBuffer) + 10,
		((char*)ioInfo->recvBuffer) + 10
		+ min(ioInfo->bodySize, (DWORD)BUF_SIZE), msg);

	// 다 복사 받았으니 할당 해제
	charPool->Free(ioInfo->recvBuffer);
	string str = string(msg);
	charPool->Free(msg);
	
	return str;
}

// 패킷 데이터 읽기
short BusinessService::PacketReading(LPPER_IO_DATA ioInfo, short bytesTrans) {
	// IO 완료후 동작 부분
	
	if (READ == ioInfo->serverMode) {
		if (bytesTrans >= 2) {
			copy(ioInfo->buffer, ioInfo->buffer + 2,
				(char*)&(ioInfo->bodySize));
		
			CharPool* charPool = CharPool::getInstance();
			ioInfo->recvBuffer = charPool->Malloc(); // 512 Byte까지 카피 가능
			if (bytesTrans - ioInfo->bodySize > 0) { // 패킷 뭉쳐있는 경우
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
			else { // 바디 내용 부족 => RecvMore에서 받을 부분 복사
				copy(ioInfo->buffer, ioInfo->buffer + bytesTrans, ((char*)ioInfo->recvBuffer));
				ioInfo->recvByte = bytesTrans;
				return bytesTrans - ioInfo->bodySize;
			}
		}
		else if(bytesTrans == 1) { // 헤더 부족
			copy(ioInfo->buffer, ioInfo->buffer + bytesTrans,
				(char*) &(ioInfo->bodySize) ); // 가지고있는 Byte까지 카피
			ioInfo->recvByte = bytesTrans;
			ioInfo->totByte = 0;
			return -1;
		}
		else {
			return 0;
		}
	} else { // 더 읽기 (BodySize짤린경우)
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

// 로그인 여부 체크 후 반환
bool BusinessService::SessionCheck(SOCKET sock) {
	// userMap접근을 위한CS
	EnterCriticalSection(&this->userCs);
	int cnt = userMap.count(sock);
	LeaveCriticalSection(&this->userCs);

	if (cnt > 0) {
		return true;
	} else {
		return false;
	}
}

// 클라이언트의 상태정보 반환
int BusinessService::GetStatus(SOCKET sock) {
	EnterCriticalSection(&this->userCs);
	int status = userMap.find(sock)->second.status;
	LeaveCriticalSection(&this->userCs);
	return status;
}

IocpService::IocpService* BusinessService::getIocpService() {
	return this->iocpService;
}

} /* namespace Service */
