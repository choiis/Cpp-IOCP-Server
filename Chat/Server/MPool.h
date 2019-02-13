/*
 * MPool.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#ifndef MPOOL_H_
#define MPOOL_H_

#include <iostream>
#include <winsock2.h>
#include <concurrent_queue.h>
#include "common.h"

class MPool {
private:
	char* data;
	DWORD len;
	Concurrency::concurrent_queue<char*> poolQueue;
	MPool();
	~MPool();
	static MPool* instance; // Singleton Instance
public:
	// Singleton Instance 를 반환
	static MPool* getInstance() {
		if (instance == nullptr) {
			cout << "ioInfo 메모리풀 10000개 할당!" << endl;
			instance = new MPool();
		}
		return instance;
	}
	// 메모리풀 할당
	LPPER_IO_DATA Malloc();
	// 메모리풀 반환
	void Free(LPPER_IO_DATA freePoint);
};

#endif /* MPOOL_H_ */
