/*
* BusinessService.cpp
*
*  Created on: 2019. 1. 17.
*      Author: choiis1207
*/

#include "BusinessService.h"

namespace BusinessService {

	BusinessService::BusinessService() {

		// 임계영역 Object 생성
		InitializeCriticalSection(&idCs);

		InitializeCriticalSection(&roomCs);

		iocpService = new IocpService::IocpService();

		fileService = new FileService::FileService();


		this->func[1] = &BusinessService::StatusLogout;
		this->func[2] = &BusinessService::StatusWait;
		this->func[3] = &BusinessService::StatusChat;

		this->directionFunc[1] = &BusinessService::UserMake;
		this->directionFunc[2] = &BusinessService::UserEnter;
		this->directionFunc[3] = &BusinessService::RoomMake;
		this->directionFunc[4] = &BusinessService::RoomEnter;

		this->directionFunc[6] = &BusinessService::Whisper;
		this->directionFunc[7] = &BusinessService::RoomInfo;
		this->directionFunc[8] = &BusinessService::RoomUserInfo;
	}

	BusinessService::~BusinessService() {

		delete iocpService;

		delete fileService;

		// 임계영역 Object 반환
		DeleteCriticalSection(&idCs);

		DeleteCriticalSection(&roomCs);

	}


	void BusinessService::callFuncPointer(SOCKET sock, ClientStatus status, Direction direction, const char* message) {

		(this->*func[status])(sock, status, direction, message);
	};
	

	void BusinessService::Sendwork() {

		Send_DATA sendData;
		if (!sendQueue.empty()) {

			queue<Send_DATA> copySendQueue;
			{
				lock_guard<mutex> guard(sendCs);  // Lock최소화
				copySendQueue = sendQueue;
				queue<Send_DATA> emptyQueue; // 빈 큐
				swap(sendQueue, emptyQueue); // 빈 큐로 바꿔치기
			}

			while (!copySendQueue.empty()) { // 여러 패킷 데이터 한꺼번에 처리
				Send_DATA sendData = copySendQueue.front();
				copySendQueue.pop();
				
				switch (sendData.direction)
				{
				case SendTo::SEND_ME: // Send to One
					iocpService->SendToOneMsg(sendData.msg.c_str(), sendData.mySocket, sendData.status);
					break;
				case SendTo::SEND_ROOM: // Send to Room
					// LockCount가 있을 때 => 방 리스트가 살아있을 때
				{
					EnterCriticalSection(&roomCs);
					auto iter = roomMap.find(sendData.roomName);
					shared_ptr<ROOM_DATA> second = nullptr;
					if (iter != roomMap.end()) {
						second = iter->second;
					}
					LeaveCriticalSection(&roomCs);

					if (second != nullptr) {
						lock_guard<recursive_mutex> guard(second->listCs);
						iocpService->SendToRoomMsg(sendData.msg.c_str(), second->userList, sendData.status);
					}
					break;
				}
				case SendTo::SEND_FILE:
				{
					 string dir = sendData.msg;
					 FILE* fp = fopen(dir.c_str(), "rb");
					if (fp == NULL) {
						cout << "파일 열기 실패" << endl;
						return;
					}

					int idx;
					while ((idx = static_cast<int>(dir.find("/"))) != -1) { // 파일명만 추출
						dir.erase(0, idx + 1);
					}

					EnterCriticalSection(&roomCs);
					auto iter = roomMap.find(sendData.roomName);
					shared_ptr<ROOM_DATA> second = nullptr;
					if (iter != roomMap.end()) {
						second = iter->second;
					}
					LeaveCriticalSection(&roomCs);

					// 각 방의 CS
					if (second != nullptr) {
						lock_guard<recursive_mutex> guard(second->listCs);
						fileService->SendToRoomFile(fp, dir, second, liveSocket);
					}
					fclose(fp);
					break;
				}
				default:
					break;
				}
			}
		}
		else {
		
			Sleep(1);
		}

	}

	// InsertSendQueue 공통화
	void BusinessService::InsertSendQueue(SendTo direction, const string& msg, const string& roomName, SOCKET socket, ClientStatus status) {

		// SendQueue에 Insert
		Send_DATA sendData;
		if (direction == SendTo::SEND_ME) {
			sendData.direction = direction;
			sendData.msg = msg;
			sendData.mySocket = socket;
			sendData.status = status;
		}
		else { //  SendTo::SEND_ROOM
			sendData.direction = direction;
			sendData.msg = msg;
			sendData.roomName = roomName;
			sendData.status = status;
		}
		{
			lock_guard<mutex> guard(sendCs);
			sendQueue.push(sendData);
		}
	
	}

	// 초기 로그인
	// 세션정보 추가
	void BusinessService::InitUser(const char *id, SOCKET sock, const char* nickName) {

		string msg;

		PER_HANDLE_DATA userInfo;
		// 유저의 상태 정보 초기화
		userInfo.status = ClientStatus::STATUS_WAITING;

		strncpy(userInfo.userId, id, NAME_SIZE);
		strncpy(userInfo.roomName, "", NAME_SIZE);
		strncpy(userInfo.userName, nickName, NAME_SIZE);

		{
			lock_guard<mutex> guard(userCs);  // Lock최소화
			// 세션정보 insert
			this->userMap[sock] = userInfo;
			cout << "START user : " << nickName << endl;
			cout << "Now login user count : " << userMap.size() << endl;
		}

		LogVo vo; // DB SQL문에 필요한 Data
		vo.setUserId(userInfo.userId);
		vo.setNickName(userInfo.userName);
		
		Dao::GetInstance()->UpdateUser(vo);// 최근 로그인기록 업데이트
		Dao::GetInstance()->InsertLogin(vo); // 로그인 DB에 기록
		
		msg = nickName;
		msg.append("님 입장을 환영합니다!\n");
		msg.append(waitRoomMessage);
		InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_WAITING);
	}

	// 접속 강제종료 로직
	void BusinessService::ClientExit(SOCKET sock) {

		if (closesocket(sock) != SOCKET_ERROR) {
			cout << "정상 종료 " << endl;

			// 콘솔 강제종료 처리
			{
				lock_guard<mutex> guard(liveSocketCs);
				liveSocket.erase(sock);
			}

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
				if (userMap.find(sock)->second.status == ClientStatus::STATUS_CHATTIG) { // 방에 접속중인 경우
					string sendMsg;
					string roomName;
					sendMsg = name;
					sendMsg += " 님이 나갔습니다!";

					// 방이름 임시 저장
					roomName = string(userMap.find(sock)->second.roomName);

					// 개별 퇴장시에는 Room List 개별 Lock만
					{
						lock_guard<recursive_mutex> guard(roomMap.find(roomName)->second->listCs);
						// 나갈때는 즉시 BoardCast
						iocpService->SendToRoomMsg(sendMsg.c_str(), roomMap.find(roomName)->second->userList, ClientStatus::STATUS_CHATTIG);

						roomMap.find(roomName)->second->userList.remove(sock); // 나가는 사람 정보 out
						// Room List 개별 Lock만
					}
					
					{
						lock_guard<mutex> guard(userCs);  // Lock최소화
						strncpy(userMap.find(sock)->second.roomName, "", NAME_SIZE); // 방이름 초기화
						userMap.find(sock)->second.status = ClientStatus::STATUS_WAITING; // 상태 변경
					}

					// room Lock은 방 완전 삭제시 에만
					EnterCriticalSection(&roomCs);
					if (roomMap.find(roomName) != roomMap.end()) {
						if ((roomMap.find(roomName)->second)->userList.size() == 0) { // 방인원 0명이면 방 삭제
							roomMap.erase(roomName);
						}
					}
					LeaveCriticalSection(&roomCs);
				}
				{
					lock_guard<mutex> guard(userCs);  // Lock최소화
					userMap.erase(sock); // 접속 소켓 정보 삭제
					cout << "Now login user count : " << userMap.size() << endl;
				}

			}

		}
	}

	// 로그인 이전 로직처리
	// 세션값 없을 때 로직
	void BusinessService::StatusLogout(SOCKET sock, ClientStatus status, Direction direction, const char *message) {

		string msg = "";

		if (direction == Direction::USER_MAKE || direction == Direction::USER_ENTER) { // 1번 계정생성 2번 로그인 시도

			(this->*directionFunc[direction])(sock, status, direction, message);
		}
		else if (direction == Direction::EXIT) {
			// 정상종료
			exit(EXIT_SUCCESS);
		}
		else { // 그외 명령어 입력
			string sendMsg = errorMessage;
			sendMsg += loginBeforeMessage;
			InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_LOGOUT);
		}

	}

	// 대기실에서의 로직 처리
	// 세션값 있음
	void BusinessService::StatusWait(SOCKET sock, ClientStatus status, Direction direction, const char *message) {

		string name = userMap.find(sock)->second.userName;
		string id = userMap.find(sock)->second.userId;
		string msg = string(message);
		// 세션에서 이름 정보 리턴
		if (direction == -1) {
			return;
		}
		else if (direction == Direction::ROOM_MAKE || direction == Direction::ROOM_ENTER || direction == Direction::WHISPER
			|| direction == Direction::ROOM_INFO || direction == Direction::ROOM_USER_INFO) { 
			(this->*directionFunc[direction])(sock, status, direction, message);
		
		}
		else if (direction == Direction::FRIEND_INFO) { // 친구 정보 요청

			RelationVo vo;
			vo.setUserId(id.c_str());
			vector<RelationVo> vec = move(Dao::GetInstance()->selectFriends(vo));

			string sendMsg = "친구 정보 리스트";
			if (vec.size() == 0) {
				sendMsg.append("\n등록된 친구가 없습니다");
				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
			}
			else {
				unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap;
				{
					lock_guard<mutex> guard(userCs);  // Lock최소화
					userCopyMap = userMap; // 로그인된 친구정보 확인위해 복사
				}

				// Select해서 가져온 유저의 친구정보
				for (int i = 0; i < vec.size(); i++) {

					unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter;
					bool exist = false;
					// userMap을 탐색하여 친구 위치 가공
					for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
						if (strcmp(vec[i].getNickName(), iter->second.userName) == 0) {
							sendMsg += "\n";
							sendMsg += vec[i].getNickName();
							sendMsg += ":";
							if (strcmp(iter->second.roomName, "") == 0) {
								sendMsg += "대기실";
							}
							else {
								sendMsg += iter->second.roomName;
							}
							exist = true;
							break;
						}
					}
					// userMap에 없으면 접속 안한 상태
					if (!exist) {
						sendMsg += "\n";
						sendMsg += vec[i].getNickName();
						sendMsg += ":";
						sendMsg += "접속안함";
					}

				}
				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
			}

		}
		else if (direction == Direction::FRIEND_ADD) { // 친구 추가
			AddFriend(sock, msg, id, ClientStatus::STATUS_WAITING);
		}
		else if (direction == Direction::FRIEND_GO) { // 친구가 있는 방으로
			RelationVo vo;
			vo.setUserId(id.c_str());
			vo.setRelationto(msg.c_str());
			// 요청 친구 정보 select
			RelationVo vo2 = move(Dao::GetInstance()->selectOneFriend(vo));

			if (strcmp(vo2.getNickName(), "") == 0) { // 친구정보 못찾음
				string sendMsg = "친구정보를 찾을 수 없습니다\n";
				sendMsg += waitRoomMessage;
				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
			}
			else {
				unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap;
				{
					lock_guard<mutex> guard(userCs);  // Lock최소화
					userCopyMap = userMap; // 로그인된 친구정보 확인위해 복사
				}

				unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter;

				bool exist = false;

				for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
					if (strcmp(vo2.getNickName(), iter->second.userName) == 0) { // 일치 닉네임 찾음
						if (strcmp(iter->second.roomName, "") == 0) { // 대기실에 있음
							string sendMsg = string(message);
							sendMsg += " 님은 대기실에 있습니다";

							InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
							exist = true;
							break;
						}
						else { // 방 입장 프로세스
							EnterCriticalSection(&roomCs);
							string roomName = iter->second.roomName;
							auto iter = roomMap.find(roomName);
							shared_ptr<ROOM_DATA> second = nullptr;
							if (iter != roomMap.end()) {
								second = iter->second;
							}
							LeaveCriticalSection(&roomCs); // 개별방 입장과 전체 방 Lock 따로

							if (second != nullptr) { // 방이 살아있을 때 상태 업데이트 + list insert
								string nick;
								{
									lock_guard<mutex> guard(userCs);  // Lock최소화
									strncpy(userMap.find(sock)->second.roomName, roomName.c_str(),
										NAME_SIZE); // 로그인 유저 정보 변경
									userMap.find(sock)->second.status = ClientStatus::STATUS_CHATTIG;
									nick = userMap.find(sock)->second.userName;
								}
								{
									lock_guard<recursive_mutex> guard(roomMap.find(msg)->second->listCs);
									roomMap.find(roomName)->second->userList.push_back(sock);
								}// 방의 Lock
								
								string sendMsg = "";
								sendMsg.append(nick);
								sendMsg += " 님이 입장하셨습니다. ";
								sendMsg += chatRoomMessage;

								InsertSendQueue(SendTo::SEND_ROOM, sendMsg, roomName, 0, ClientStatus::STATUS_CHATTIG);
							}

							exist = true;
							break;
						}
					}
				}

				if (!exist) {
					string sendMsg = string(message);
					sendMsg += " 님은 접속중이 아닙니다";

					InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
				}
			}
		}
		else if (direction == Direction::FRIEND_DELETE) { // 친구 삭제

			RelationVo vo;
			vo.setUserId(id.c_str());
			vo.setRelationcode(1);
			vo.setNickName(msg.c_str());

			int res = Dao::GetInstance()->DeleteRelation(vo);
			if (res != -1) { // 삭제 성공
				string sendMsg = msg;
				sendMsg += " 님을 친구 삭제했습니다";

				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
			}
			else { // 삭제실패
				string sendMsg = "친구 삭제실패\n";
				sendMsg.append(waitRoomMessage);

				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
			}
		}
		else if (direction == Direction::LOG_OUT) { // 로그아웃
			char id[NAME_SIZE];
			EnterCriticalSection(&idCs);
			strncpy(id, userMap.find(sock)->second.userId, NAME_SIZE);
			idSet.erase(userMap.find(sock)->second.userId); // 로그인 셋에서 제외
			LeaveCriticalSection(&idCs);

			{
				lock_guard<mutex> guard(userCs);  // Lock최소화
				userMap.erase(sock); // 접속 소켓 정보 삭제
				cout << "Now login user count : " << userMap.size() << endl;
			}

			string sendMsg = loginBeforeMessage;
			InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_LOGOUT);
		}
		else if (direction == Direction::USER_GOOD_INFO) { // 인기도 조회
			vector<RankVo> vec = move(Redis::GetInstance()->Zrevrange("pl", 20));
			string sendMsg = "인기도 리스트";
			vector<RankVo>::iterator iter;
			for (iter = vec.begin(); iter != vec.end(); iter++)
			{
				sendMsg += "\n";
				sendMsg += iter->getNickName();
				sendMsg += " :";
				sendMsg += to_string(iter->getPoint());
			}
			InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
		}
		else { // 그외 명령어 입력
			string sendMsg = errorMessage;
			sendMsg += waitRoomMessage;
			// 대기방의 오류이므로 ClientStatus::STATUS_WAITING 상태로 전달한다

			InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
		}

		LogVo vo; // DB SQL문에 필요한 Data
		vo.setNickName(name.c_str());
		vo.setMsg(message);
		vo.setDirection(direction);
		vo.setStatus(status);
		Dao::GetInstance()->InsertDirection(vo);  // 지시 기록 insert
		
	}

	// 채팅방에서의 로직 처리
	// 세션값 있음
	void BusinessService::StatusChat(SOCKET sock, ClientStatus status, Direction direction, const char *message) {

		string name;
		string msg;
		string id;
		// 세션에서 이름 정보 리턴
		name = userMap.find(sock)->second.userName;
		id = userMap.find(sock)->second.userId;
		msg = string(message);

		if (msg.compare("\\out") == 0) { // 채팅방 나감

			string sendMsg;
			string roomName;
			sendMsg = name;
			sendMsg += " 님이 나갔습니다!";

			// 방이름 임시 저장
			roomName = string(userMap.find(sock)->second.roomName);

			// 개별 퇴장시에는 Room List 개별 Lock만
			{
				lock_guard<recursive_mutex> guard(roomMap.find(roomName)->second->listCs);
				// 나갈때는 즉시 BoardCast
				iocpService->SendToRoomMsg(sendMsg.c_str(), roomMap.find(roomName)->second->userList, ClientStatus::STATUS_CHATTIG);

				roomMap.find(roomName)->second->userList.remove(sock); // 나가는 사람 정보 out
			}
			// Room List 개별 Lock만

			msg = waitRoomMessage;
			InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_WAITING);

			{
				lock_guard<mutex> guard(userCs);  // Lock최소화 // 로그인된 사용자정보 변경 Lock
				strncpy(userMap.find(sock)->second.roomName, "", NAME_SIZE); // 방이름 초기화
				userMap.find(sock)->second.status = ClientStatus::STATUS_WAITING; // 상태 변경
			}

			// room Lock은 방 완전 삭제시 에만
			EnterCriticalSection(&roomCs);
			if (roomMap.find(roomName) != roomMap.end()) {
				if ((roomMap.find(roomName)->second)->userList.size() == 0) { // 방인원 0명이면 방 삭제
					roomMap.erase(roomName);
				}
			}
			LeaveCriticalSection(&roomCs);
		}
		else if (msg.find("\\add") != -1) { // 친구추가

			msg.erase(0, 5); // 친구 이름 반환
			// 친구추가
			AddFriend(sock, msg, id, ClientStatus::STATUS_CHATTIG);
		}
		else if (direction == USER_GOOD) { // 인기도

			// 방이름 임시 저장
			string roomName = string(userMap.find(sock)->second.roomName);

			unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap;
			{
				lock_guard<mutex> guard(userCs);  // Lock최소화
				userCopyMap = userMap; // 로그인된 친구정보 확인위해 복사
			}

			unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter;
			bool find = false;
			for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
				if (strcmp(msg.c_str(), iter->second.userName) == 0) {
					Redis::GetInstance()->Zincr("pl", msg);
					find = true;
					break;
				}
			}
			
			if (find) {
				string sendMsg = "인기도 추천 성공\n";
				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_CHATTIG);
			}
			else {
				string sendMsg = "인기도 추천 실패\n";
				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_CHATTIG);
			}
		}
		else if (direction == FILE_SEND) {
			string fileDir = move(fileService->RecvFile(sock, string(userMap.find(sock)->second.userName)));

			// SendQueue에 Insert
			InsertSendQueue(SendTo::SEND_ROOM, "", userMap.find(sock)->second.roomName, 0, ClientStatus::STATUS_FILE_SEND);
			// 여기서부터 UDP BroadCast
			InsertSendQueue(SendTo::SEND_FILE, fileDir.c_str(), userMap.find(sock)->second.roomName, 0, ClientStatus::STATUS_FILE_SEND);
		}
		else { // 채팅방에서 채팅중

			string sendMsg;
			sendMsg = name;
			sendMsg += ":";
			sendMsg += msg;

			char roomName[NAME_SIZE];

			unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap;
			map<string, shared_ptr<ROOM_DATA>>::iterator it;
			{
				lock_guard<mutex> guard(userCs);  // Lock최소화
				it = roomMap.find(
					userMap.find(sock)->second.roomName);
				strncpy(roomName, userMap.find(sock)->second.roomName, NAME_SIZE);
			}

			if (it != roomMap.end()) { // null 검사
				InsertSendQueue(SendTo::SEND_ROOM, sendMsg, userMap.find(sock)->second.roomName, 0, ClientStatus::STATUS_CHATTIG);
			}

			LogVo vo; // DB SQL문에 필요한 Data
			vo.setNickName(name.c_str());
			vo.setRoomName(roomName);
			vo.setMsg(msg.c_str());
			vo.setDirection(0);
			vo.setStatus(0);
			Dao::GetInstance()->InsertChatting(vo);
		
		}
	}

	// 클라이언트에게 받은 데이터 복사후 구조체 해제
	// 사용 메모리 전부 반환
	string BusinessService::DataCopy(LPPER_IO_DATA ioInfo, ClientStatus *status, Direction *direction) {

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

		if (iocpService->RECV == ioInfo->serverMode) {
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
			ioInfo->serverMode = iocpService->RECV;
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

	// 클라이언트의 상태정보 반환
	ClientStatus BusinessService::GetStatus(SOCKET sock) {
		
		lock_guard<mutex> guard(userCs);  // Lock최소화
	
		unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter = userMap.find(sock);
		if (iter == userMap.end()) {
			return ClientStatus::STATUS_LOGOUT;
		}
		else {
			ClientStatus status = userMap.find(sock)->second.status;
			return status;
		}
	}

	// 계정만들기
	void BusinessService::UserMake(SOCKET sock, ClientStatus status, Direction direction, const char* message) {
		
		string msg = "";
		char* sArr[3] = { nullptr, };
		char message2[BUF_SIZE];
		strncpy(message2, message, BUF_SIZE);
		char* ptr = strtok(message2, "\\"); // 공백 문자열을 기준으로 문자열을 자름

		int i = 0;
		while (ptr != nullptr)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(nullptr, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		// 유효성 검증 필요
		if (sArr[0] != nullptr && sArr[1] != nullptr && sArr[2] != nullptr) {

			UserVo vo;
			vo.setUserId(sArr[0]);
			vo = move(Dao::GetInstance()->selectUser(vo));

			if (strcmp(vo.getUserId(), "") == 0) { // ID 중복체크 => 계정 없음

				vo.setUserId(sArr[0]);
				vo.setPassword(sArr[1]);
				vo.setNickName(sArr[2]);

				Dao::GetInstance()->InsertUser(vo);

				msg.append(sArr[0]);
				msg.append("계정 생성 완료!\n");
				msg.append(loginBeforeMessage);

				InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_LOGOUT);

			}
			else { // ID중복있음

				msg.append("중복된 아이디가 있습니다!\n");
				msg.append(loginBeforeMessage);

				InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_LOGOUT);
			}
		}
	}

	// 유저입장
	void BusinessService::UserEnter(SOCKET sock, ClientStatus status, Direction direction, const char* message) {
		string msg = "";

		char* sArr[2] = { nullptr, };
		char message2[BUF_SIZE];
		strncpy(message2, message, BUF_SIZE);
		char* ptr = strtok(message2, "\\"); // 공백 문자열을 기준으로 문자열을 자름

		int i = 0;
		while (ptr != nullptr)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(nullptr, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		if (sArr[0] != nullptr && sArr[1] != nullptr) {

			UserVo vo;
			vo.setUserId(sArr[0]);
			vo = move(Dao::GetInstance()->selectUser(vo));

			if (strcmp(vo.getUserId(), "") == 0) { // 계정 없음

				msg.append("없는 계정입니다 아이디를 확인하세요!\n");
				msg.append(loginBeforeMessage);

				// SendQueue에 Insert
				InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_LOGOUT);

			}
			else if (strcmp(vo.getPassword(), sArr[1]) == 0) { // 비밀번호 일치
				EnterCriticalSection(&idCs);
				unordered_set<string>::const_iterator it = idSet.find(sArr[0]);
				if (it != idSet.end()) { // 중복로그인 유효성 검사
					LeaveCriticalSection(&idCs); // Case Lock 해제
					msg.append("중복 로그인은 안됩니다!\n");
					msg.append(loginBeforeMessage);

					// SendQueue에 Insert
					InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_LOGOUT);

				}
				else { // 중복로그인 X
					idSet.insert(sArr[0]);
					LeaveCriticalSection(&idCs); //Init부분 두번동작 방지
					InitUser(sArr[0], sock, vo.getNickName()); // 세션정보 저장
				}

			}
			else { // 비밀번호 틀림
				msg.append("비밀번호 틀림!\n");
				msg.append(loginBeforeMessage);

				InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_LOGOUT);
			}
		}
	}
	
	// 방 만들기 기능
	void BusinessService::RoomMake(SOCKET sock, ClientStatus status, Direction direction, const char* message) {

		string name = userMap.find(sock)->second.userName;
		string id = userMap.find(sock)->second.userId;
		string msg = string(message);

		// 유효성 검증 먼저
		EnterCriticalSection(&roomCs);
		size_t cnt = roomMap.count(msg);

		if (cnt != 0) { // 방이름 중복
			LeaveCriticalSection(&roomCs);
			msg.append("이미 있는 방 이름입니다!\n");
			msg.append(waitRoomMessage);
			InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_WAITING);
			// 중복 케이스는 방 만들 수 없음
		}
		else { // 방이름 중복 아닐 때만 개설
			// 새로운 방 정보 저장
			shared_ptr<ROOM_DATA> roomData = make_shared<ROOM_DATA>();
			list<SOCKET> chatList;
			chatList.push_back(sock);
			// 방 리스트별 CS객체 Init
			roomData->userList = chatList;
			roomMap[msg] = roomData;

			LeaveCriticalSection(&roomCs);

			// User의 상태 정보 바꾼다
			{
				lock_guard<mutex> guard(userCs);  // Lock최소화
				strncpy((userMap.find(sock))->second.roomName, msg.c_str(),
					NAME_SIZE);
				(userMap.find(sock))->second.status =
					ClientStatus::STATUS_CHATTIG;
			}

			msg += " 방이 개설되었습니다.";
			msg += chatRoomMessage;
			cout << "Now server room count : " << roomMap.size() << endl;
			InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_CHATTIG);

		}
	}

	// 방 들어가기
	void BusinessService::RoomEnter(SOCKET sock, ClientStatus status, Direction direction, const char* message) {

		string name = userMap.find(sock)->second.userName;
		string id = userMap.find(sock)->second.userId;
		string msg = string(message);

		// 유효성 검증 먼저
		EnterCriticalSection(&roomCs);

		if (roomMap.find(msg) == roomMap.end()) { // 방 못찾음
			LeaveCriticalSection(&roomCs);
			msg.append("없는 방 입니다!\n");
			msg.append(waitRoomMessage);

			InsertSendQueue(SendTo::SEND_ME, msg, "", sock, ClientStatus::STATUS_WAITING);

		}
		else {

			auto iter = roomMap.find(msg);
			shared_ptr<ROOM_DATA> second = nullptr;
			if (iter != roomMap.end()) {
				second = iter->second;
			}
			LeaveCriticalSection(&roomCs); // 개별방 입장과 전체 방 Lock 따로

			if (second != nullptr) { // 방이 살아있을 때 상태 업데이트 + list insert

				{
					lock_guard<mutex> guard(userCs);  // Lock최소화
					strncpy(userMap.find(sock)->second.roomName, msg.c_str(),
						NAME_SIZE); // 로그인 유저 정보 변경
					userMap.find(sock)->second.status = ClientStatus::STATUS_CHATTIG;
				}
				{
					lock_guard<recursive_mutex> guard(roomMap.find(msg)->second->listCs);
					//방이 있으니까 유저를 insert
					roomMap.find(msg)->second->userList.push_back(sock);
				}
				// 방의 Lock

				string sendMsg = name;
				sendMsg.append(" 님이 입장하셨습니다. ");
				sendMsg.append(chatRoomMessage);

				InsertSendQueue(SendTo::SEND_ROOM, sendMsg, msg, 0, ClientStatus::STATUS_CHATTIG);
			}
		}
	}

	// 귓속말
	void BusinessService::Whisper(SOCKET sock, ClientStatus status, Direction direction, const char* message) {

		string name = userMap.find(sock)->second.userName;
		string id = userMap.find(sock)->second.userId;
		string msg = string(message);

		char* sArr[2] = { nullptr, };
		char message2[BUF_SIZE];
		strncpy(message2, message, BUF_SIZE);
		char* ptr = strtok(message2, "\\"); // 공백 문자열을 기준으로 문자열을 자름
		int i = 0;
		while (ptr != nullptr)            // 자른 문자열이 나오지 않을 때까지 반복
		{
			sArr[i] = ptr;           // 문자열을 자른 뒤 메모리 주소를 문자열 포인터 배열에 저장
			i++;                       // 인덱스 증가
			ptr = strtok(nullptr, "\\");   // 다음 문자열을 잘라서 포인터를 반환
		}
		if (sArr[0] != nullptr && sArr[1] != nullptr) {
			name = string(sArr[0]);
			msg = string(sArr[1]);

			string sendMsg;

			if (name.compare(userMap.find(sock)->second.userName) == 0) { // 본인에게 쪽지
				sendMsg = "자신에게 쪽지를 보낼수 없습니다\n";
				sendMsg += waitRoomMessage;

				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);

			}
			else {
				bool find = false;
				unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap;
				{
					lock_guard<mutex> guard(userCs);  // Lock최소화
					userCopyMap = userMap; // 로그인된 친구정보 확인위해 복사
				}

				unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter;

				for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
					if (name.compare(iter->second.userName) == 0) {
						find = true;
						sendMsg = userCopyMap.find(sock)->second.userName;
						sendMsg += " 님에게 온 귓속말 : ";
						sendMsg += msg;

						InsertSendQueue(SendTo::SEND_ME, sendMsg, "", iter->first, ClientStatus::STATUS_WHISPER);

						break;
					}
				}

				// 귓속말 대상자 못찾음
				if (!find) {
					sendMsg = name;
					sendMsg += " 님을 찾을 수 없습니다";

					InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, ClientStatus::STATUS_WAITING);
				}
			}
		}

	}

	// 방 정보
	void BusinessService::RoomInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message) {

		string name = userMap.find(sock)->second.userName;
		string id = userMap.find(sock)->second.userId;
		string msg = string(message);

		string str;
		if (roomMap.size() == 0) {
			str = "만들어진 방이 없습니다";
		}
		else {
			str += "방 정보 리스트";
			EnterCriticalSection(&roomCs);
			map<string, shared_ptr<ROOM_DATA>> roomCopyMap = roomMap;
			LeaveCriticalSection(&roomCs);
			// roomMap 계속 잡고 있지 않도록 깊은복사
			map<string, shared_ptr<ROOM_DATA>>::const_iterator iter;

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

		InsertSendQueue(SendTo::SEND_ME, str, "", sock, ClientStatus::STATUS_WAITING);
	}

	// 유저 방 정보
	void BusinessService::RoomUserInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message) {
		string name = userMap.find(sock)->second.userName;
		string id = userMap.find(sock)->second.userId;
		string msg = string(message);

		string str = "유저 정보 리스트";
		unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap;
		{
			lock_guard<mutex> guard(userCs);  // Lock최소화
			userCopyMap = userMap;
		}
		// userMap 계속 잡고 있지 않도록 깊은복사
		unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter;
		for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
			if (str.length() >= 4000) {
				break;
			}
			str += "\n";
			str += (iter->second).userName;
			str += ":";
			if ((iter->second).status == ClientStatus::STATUS_WAITING) {
				str += "대기실";
			}
			else {
				str += (iter->second).roomName;
			}
		}

		InsertSendQueue(SendTo::SEND_ME, str, "", sock, ClientStatus::STATUS_WAITING);
	}

	// 친구추가 기능
	void BusinessService::AddFriend(SOCKET sock, const string& msg, const string& id, ClientStatus status) {
		RelationVo vo;
		vo.setNickName(msg.c_str());
		RelationVo vo2 = move(Dao::GetInstance()->findUserId(vo)); // 아이디 존재 여부 검색

		string sendMsg;

		if (strcmp(vo2.getRelationto(), "") == 0) {
			sendMsg = "아이디 없음 친추 불가능";
			InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, status);
		}
		else {

			vo2.setUserId(id.c_str());
			vo2.setRelationcode(1);
			int res = Dao::GetInstance()->InsertRelation(vo2);
			if (res != -1) { // 친추 성공
				sendMsg = msg;
				sendMsg.append("님이 친구추가 되었습니다");
				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, status);
			}
			else {
				sendMsg = msg;
				sendMsg.append("님이 친구추가 실패");
				InsertSendQueue(SendTo::SEND_ME, sendMsg, "", sock, status);
			}
		}
	}

	void BusinessService::InsertLiveSocket(const SOCKET& hClientSock, const SOCKADDR_IN& addr) {
		{
			lock_guard<mutex> guard(liveSocketCs);
			liveSocket.insert(pair<SOCKET, string>(hClientSock, inet_ntoa(addr.sin_addr)));
		}
	}

	bool BusinessService::IsSocketDead(SOCKET socket){
		return liveSocket.find(socket) == liveSocket.end();
	}

	void BusinessService::BanUser(SOCKET socket, const char* nickName) {

		unordered_map<SOCKET, PER_HANDLE_DATA> userCopyMap;
		{
			lock_guard<mutex> guard(userCs);  // Lock최소화
			userCopyMap = userMap; // 로그인된 친구정보 확인위해 복사
		}

		unordered_map<SOCKET, PER_HANDLE_DATA>::const_iterator iter;

		for (iter = userCopyMap.begin(); iter != userCopyMap.end(); iter++) {
			if (strcmp(nickName, iter->second.userName) == 0) {
				SOCKET sock = iter->first;

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
				if (userMap.find(sock)->second.status == ClientStatus::STATUS_CHATTIG) { // 방에 접속중인 경우
					string sendMsg;
					string roomName;
					sendMsg = name;
					sendMsg += " 님이 나갔습니다!";

					// 방이름 임시 저장
					roomName = string(userMap.find(sock)->second.roomName);

					// 개별 퇴장시에는 Room List 개별 Lock만
					{
						lock_guard<recursive_mutex> guard(roomMap.find(roomName)->second->listCs);
						// 나갈때는 즉시 BoardCast
						iocpService->SendToRoomMsg(sendMsg.c_str(), roomMap.find(roomName)->second->userList, ClientStatus::STATUS_CHATTIG);

						roomMap.find(roomName)->second->userList.remove(sock); // 나가는 사람 정보 out

					}				
					// Room List 개별 Lock만


					{
						lock_guard<mutex> guard(userCs);  // 로그인된 사용자정보 변경 Lock
						strncpy(userMap.find(sock)->second.roomName, "", NAME_SIZE); // 방이름 초기화
						userMap.find(sock)->second.status = ClientStatus::STATUS_WAITING; // 상태 변경
					}

					// room Lock은 방 완전 삭제시 에만
					EnterCriticalSection(&roomCs);
					if (roomMap.find(roomName) != roomMap.end()) {
						if ((roomMap.find(roomName)->second)->userList.size() == 0) { // 방인원 0명이면 방 삭제
							roomMap.erase(roomName);
						}
					}
					LeaveCriticalSection(&roomCs);
				}
				string str = "당신은 관리자에의해 강퇴되었습니다";
				InsertSendQueue(SendTo::SEND_ME, str.c_str(), "", sock, ClientStatus::STATUS_LOGOUT);

				{
					lock_guard<mutex> guard(userCs);
					userMap.erase(sock); // 접속 소켓 정보 삭제
					cout << "Now login user count : " << userMap.size() << endl;
				}

				break;
			}
		}
	}

	void BusinessService::CallCnt(SOCKET socket, const DWORD& cnt) {
		char msg[30];
		sprintf(msg, "{\"packet\":%d , \"cnt\":%zd}", cnt, userMap.size());
		InsertSendQueue(SendTo::SEND_ME, msg, "", socket, ClientStatus::STATUS_INIT);
	}

	IocpService::IocpService* BusinessService::getIocpService() {
		return this->iocpService;
	}

} /* namespace Service */