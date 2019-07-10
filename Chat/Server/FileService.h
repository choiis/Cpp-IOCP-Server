
#ifndef FILESERVICE_H_
#define FILESERVICE_H_

#include <winsock2.h>
#include <memory>
#include <algorithm>
#include <iostream>
#include <direct.h>
#include <list>
#include <string>
#include <map>
#include "common.h"

namespace FileService {

class FileService {
private:
	// udp 소켓 초기화에 필요
	WSADATA wsaData;
	// udp 소켓
	SOCKET udpSocket;
	// udp Send 소켓
	SOCKET udpSendSocket;

	FileService(const FileService& rhs) = delete;
	void operator=(const FileService& rhs) = delete;
	FileService(FileService&& rhs) = delete;
public:
	// 생성자
	FileService();
	// 소멸자
	virtual ~FileService();
	// UDP파일전송
	// 메서드밖에서 방 리스트에대한 동기화 
	void SendToRoomFile(FILE* fp, const string& dir, shared_ptr<ROOM_DATA> second, const map<SOCKET, string>& liveSocket);
	// 채팅방에서의 파일 입출력 케이스
	string  RecvFile(SOCKET sock);
};

}/* namespace Service */

#endif /* FILESERVICE_H_ */
