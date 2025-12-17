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
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <direct.h>

#include "IocpService.h"
#include "FileService.h"
#include "Dao.h"

 // 서버에 접속한 유저 정보
 // client 소켓에 대응하는 세션정보
typedef struct { // socket info
    char userName[NAME_SIZE];
    char roomName[NAME_SIZE];
    char userId[NAME_SIZE];
    ClientStatus status;
} PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

namespace BusinessService {

    class BusinessService {
    private:
        // 중복로그인 방지에 쓰일 구조체
        std::unordered_set<std::string> idSet;

        // 서버에 접속한 유저 자료 저장
        std::unordered_map<SOCKET, PER_HANDLE_DATA> userMap;

        // 서버의 방 정보 저장
        std::map<std::string, std::shared_ptr<ROOM_DATA>> roomMap;

        // Send Queue
        std::queue<Send_DATA> sendQueue;

        // idSet 동기화 (CRITICAL_SECTION)
        CRITICAL_SECTION idCs;

        // userMap 동기화
        std::mutex userCs;

        // roomMap 동기화 (CRITICAL_SECTION)
        CRITICAL_SECTION roomCs;

        // sendQueue 동기화 + condition_variable
        std::mutex sendCs;
        std::condition_variable sendCv;          // ✅ 추가
        std::atomic<bool> sendStop{ false };       // ✅ 선택(종료 지원)

        // 접속끊어진 socket은 Send에서 제외
        // UDP 전송 Case때문에 각 클라이언트 socket의 IP도 저장한다
        std::map<SOCKET, std::string> liveSocket;
        std::mutex liveSocketCs;

        IocpService::IocpService* iocpService;
        FileService::FileService* fileService;

        BusinessService(const BusinessService& rhs) = delete;
        void operator=(const BusinessService& rhs) = delete;
        BusinessService(BusinessService&& rhs) = delete;

        // 로그인 이전 로직처리
        void StatusLogout(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 대기실에서의 로직 처리
        void StatusWait(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 채팅방에서의 로직 처리
        void StatusChat(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 계정만들기
        void UserMake(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 유저입장
        void UserEnter(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 방 만들기 기능
        void RoomMake(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 방 들어가기
        void RoomEnter(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 귓속말
        void Whisper(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 방 정보
        void RoomInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 유저 방 정보
        void RoomUserInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 친구정보 요청
        void FriendInfo(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 친구추가 기능
        void FriendAdd(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 친구에게 가기
        void FriendGo(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // 친구삭제
        void FriendDelete(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        void (BusinessService::* func[4])(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        void (BusinessService::* directionFunc[13])(SOCKET sock, ClientStatus status, Direction direction, const char* message);

    public:
        // 생성자/소멸자
        BusinessService();
        virtual ~BusinessService();

        void callFuncPointer(SOCKET sock, ClientStatus status, Direction direction, const char* message);

        // ✅ SendThread에서 동작할 부분 (CV 기반으로 최적화)
        void Sendwork();

        // InsertSendQueue 공통화 (push + notify)
        void InsertSendQueue(SendTo direction, const std::string& msg, const std::string& roomName,
            SOCKET socket, ClientStatus status);

        // (선택) 서버 종료 시 SendThread 깨우기 용
        void StopSendWorker();

        // 초기 로그인 (세션정보 추가)
        void InitUser(const char* id, SOCKET sock, const char* nickName);

        // 접속 종료 로직
        void ClientExit(SOCKET sock);

        // 클라이언트에게 받은 데이터 복사후 구조체 해제
        std::string DataCopy(LPPER_IO_DATA ioInfo, ClientStatus* status, Direction* direction);

        // 패킷 데이터 읽기
        short PacketReading(LPPER_IO_DATA ioInfo, short bytesTrans);

        // 클라이언트의 상태정보 반환
        ClientStatus GetStatus(SOCKET sock);

        // 연결중 socket Insert
        void InsertLiveSocket(const SOCKET& hClientSock, const SOCKADDR_IN& addr);

        // socket 죽었는지 확인 (liveSocketCs로 보호)
        bool IsSocketDead(SOCKET socket);

        // node 서버에서 강퇴하기
        void BanUser(SOCKET socket, const char* nickName);

        // node 서버로 로그인 유저수 반환
        void CallCnt(SOCKET socket, const DWORD& cnt);

        // getters
        const std::unordered_set<std::string>& getIdSet() const { return idSet; }
        const std::map<std::string, std::shared_ptr<ROOM_DATA>>& getRoomMap() const { return roomMap; }
        const std::unordered_map<SOCKET, PER_HANDLE_DATA>& getUserMap() const { return userMap; }

        IocpService::IocpService* getIocpService();

        const CRITICAL_SECTION& getIdCs() const { return idCs; }
        const CRITICAL_SECTION& getRoomCs() const { return roomCs; }
    };

} /* namespace BusinessService */

#endif /* BUSINESSSERVICE_H_ */
