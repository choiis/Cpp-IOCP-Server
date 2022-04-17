/*
 * BusinessService.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#ifndef BUSINESSSERVICE_H_
#define BUSINESSSERVICE_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <queue>
#include <direct.h>
#include <mutex>
#include "IocpService.h"
#include "FileService.h"
#include "Dao.h"

// ������ ������ ���� ����
// client ���Ͽ� �����ϴ� ��������
typedef struct { // socket info
	char userName[NAME_SIZE];
	char roomName[NAME_SIZE];
	char userId[NAME_SIZE];
	ClientStatus status;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;


namespace BusinessService {

class BusinessService {
private:
	// �ߺ��α��� ������ ���� ����ü
	unordered_set<string> idSet;
	// ������ ������ ���� �ڷ� ����
	unordered_map<SOCKET, PER_HANDLE_DATA> userMap;
	// ������ �� ���� ����
	map<string, shared_ptr<ROOM_DATA>> roomMap;


	queue<Send_DATA> sendQueue;
	// �Ӱ迵���� �ʿ��� ��ü
	// Ŀ�θ�� �ƴ϶� ���������� ����ȭ ����� ��
	// �� ���μ������� ����ȭ �̹Ƿ� ũ��Ƽ�ü��� ���

	// idMap ����ȭ
	CRITICAL_SECTION idCs;
	// userMap ����ȭ
	mutex userCs;
	// roomMap ����ȭ
	CRITICAL_SECTION roomCs;
	
	// sendQueue ����ȭ
	mutex sendCs;

	// ���Ӳ����� socket�� Send���� ����
	// UDP ���� Case������ �� Ŭ���̾�Ʈ socket�� IP�� �����Ѵ�
	map<SOCKET, string> liveSocket;

	mutex liveSocketCs;

	IocpService::IocpService *iocpService;

	FileService::FileService *fileService;

	BusinessService(const BusinessService& rhs) = delete;
	void operator=(const BusinessService& rhs) = delete;
	BusinessService(BusinessService&& rhs) = delete;
	// �α��� ���� ����ó��
	// ���ǰ� ���� �� ����
	void StatusLogout(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ���ǿ����� ���� ó��
	// ���ǰ� ����
	void StatusWait(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ä�ù濡���� ���� ó��
	// ���ǰ� ����
	void StatusChat(SOCKET sock, ClientStatus status, Direction direction, const char* message);


	// ���������
	void UserMake(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ��������
	void UserEnter(SOCKET sock, ClientStatus status, Direction direction, const char* message);

	// �� ����� ���
	void RoomMake(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// �� ����
	void RoomEnter(SOCKET sock, ClientStatus status, Direction direction, const char* message);

	// �ӼӸ�
	void Whisper(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// �� ����
	void RoomInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ���� �� ����
	void RoomUserInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ģ������ ��û
	void FriendInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ģ���߰� ���
	void FriendAdd(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ģ������ ����
	void FriendGo(SOCKET sock, ClientStatus status, Direction direction, const char* message);
	// ģ������
	void FriendDelete(SOCKET sock, ClientStatus status, Direction direction, const char* message);

	void (BusinessService::*func[4])(SOCKET sock, ClientStatus status, Direction direction, const char* message);

	// ���������
	void (BusinessService::* directionFunc[13])(SOCKET sock, ClientStatus status, Direction direction, const char* message);

public:

	void callFuncPointer(SOCKET sock, ClientStatus status, Direction direction, const char* message);

	// ������
	BusinessService();
	// �Ҹ���
	virtual ~BusinessService();
	
	// SendThread���� ������ �κ�
	void Sendwork();
	// InsertSendQueue ����ȭ
	void InsertSendQueue(SendTo direction, const string& msg, const string& roomName, SOCKET socket, ClientStatus status);
	// �ʱ� �α���
	// �������� �߰�
	void InitUser(const char *id, SOCKET sock ,const char *nickName);
	// ���� �������� ����
	void ClientExit(SOCKET sock);

	// Ŭ���̾�Ʈ���� ���� ������ ������ ����ü ����
	string DataCopy(LPPER_IO_DATA ioInfo, ClientStatus *status, Direction *direction);
	// ��Ŷ ������ �б�
	short PacketReading(LPPER_IO_DATA ioInfo, short bytesTrans);

	// Ŭ���̾�Ʈ�� �������� ��ȯ
	ClientStatus GetStatus(SOCKET sock);
	
	// ������ socket Insert
	void InsertLiveSocket(const SOCKET& hClientSock, const SOCKADDR_IN& addr);
	// socket �׾����� Ȯ��
	bool IsSocketDead(SOCKET socket);
	// node �������� �����ϱ�
	void BanUser(SOCKET socket, const char* nickName);
	// node ������ �α��� ������ ��ȯ
	void CallCnt(SOCKET socket, const DWORD& cnt);

	const unordered_set<string>& getIdSet() const {
		return idSet;
	}

	const map<string, std::shared_ptr<ROOM_DATA>>& getRoomMap() const {
		return roomMap;
	}

	const unordered_map<SOCKET, PER_HANDLE_DATA>& getUserMap() const {
		return userMap;
	}

	IocpService::IocpService* getIocpService() ;

	const CRITICAL_SECTION& getIdCs() const {
		return idCs;
	}

	const CRITICAL_SECTION& getRoomCs() const {
		return roomCs;
	}

};

} /* namespace Service */

#endif /* BUSINESSSERVICE_H_ */
