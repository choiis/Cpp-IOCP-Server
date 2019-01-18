//============================================================================
// Name        : MemoryPool.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <cstdlib>
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <queue>
using namespace std;

#define BUF_SIZE 500

// 비동기 통신에 필요한 구조체
typedef struct { // buffer info
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];  // 버퍼에 PACKET_DATA가 들어갈 것이므로 크기 맞춤
	int serverMode;
	DWORD recvByte; // 지금까지 받은 바이트를 알려줌, 지금까지 보낸 사람
	DWORD totByte; //전체 받을 바이트를 알려줌 , 총 보낼 사람
	int bodySize; // 패킷의 body 사이즈
	char *recvBuffer;
} PER_IO_DATA, *LPPER_IO_DATA;

class ArrayPool {
private:
	char* data;
	bool* arr;
	DWORD cnt;
	DWORD idx;
public:
	ArrayPool(DWORD size) {
		data = new char[size * sizeof(PER_IO_DATA)];
		arr = new bool[size];
		cnt = size;
		idx = 0;
		memset(arr, 0, size);
	}
	~ArrayPool() {
		cnt = 0;
		idx = 0;
		delete data;
		delete arr;
	}
	LPPER_IO_DATA malloc() {

		while (arr[idx]) {
			idx++;
			if (idx == cnt) {
				idx = 0;
			}
		}

		arr[idx] = true;
		idx++;
		if (idx == cnt) {
			idx = 0;
			return (LPPER_IO_DATA) (data + ((cnt - 1) * (sizeof(PER_IO_DATA))));
		} else {
			return (LPPER_IO_DATA) (data + ((idx - 1) * (sizeof(PER_IO_DATA))));
		}

	}

	void free(LPPER_IO_DATA freePoint) {
		DWORD returnIdx = ((((char*) freePoint) - data) / sizeof(PER_IO_DATA));
		arr[returnIdx] = false;
	}
};

class ListPool {
private:
	unsigned char *data;
	unsigned char *first;
	unsigned char *last;
public:
	ListPool(int block) {
		data = new unsigned char[sizeof(PER_IO_DATA) * block];
		first = data;
		last = &data[sizeof(PER_IO_DATA) * (block - 1)];
		unsigned char** pointer = (unsigned char**) (first);
		unsigned char* next = data;
		for (int i = 0; i < block; i++) {
			next += sizeof(PER_IO_DATA);
			*pointer = next;
			pointer = (unsigned char**) next;
		}
		*pointer = 0;
	}
	~ListPool() {
		first = nullptr;
		last = nullptr;
		delete data;
	}
	LPPER_IO_DATA malloc() {
		unsigned char* alloc = first;
		first = *reinterpret_cast<unsigned char**>(alloc);
		return (LPPER_IO_DATA) alloc;
	}

	void free(LPPER_IO_DATA p) {
		unsigned char** lastPoint = (unsigned char**) (last);
		*lastPoint = (unsigned char*) p;
		last = *reinterpret_cast<unsigned char**>(lastPoint);
	}
};

class QueuePool {
private:
	char* data;
	queue<char*> poolQueue;
public:
	QueuePool(DWORD blocks) {
		data = new char[sizeof(PER_IO_DATA) * blocks];
		DWORD i = 0;
		for (i = 0; i < blocks; i++) {
			poolQueue.push(data + (sizeof(PER_IO_DATA) * i));
		}
	}
	~QueuePool() {
		while(!poolQueue.empty()) {
			poolQueue.pop();
		}
		delete data;
	}
	LPPER_IO_DATA malloc() {
		LPPER_IO_DATA pointer = (LPPER_IO_DATA) poolQueue.front();
		poolQueue.pop();
		return pointer;
	}

	void free(LPPER_IO_DATA freePoint) {
		poolQueue.push((char*) freePoint);
	}

};

int main() {

	ListPool pool(3);

	LPPER_IO_DATA p1 = pool.malloc();
	printf("%u\n", p1);
	LPPER_IO_DATA p2 = pool.malloc();
	printf("%u\n", p2);
	LPPER_IO_DATA p3 = pool.malloc();
	printf("%u\n", p3);
	pool.free(p2);
	LPPER_IO_DATA p4 = pool.malloc();
	printf("%u\n", p4);
	return 0;
}
