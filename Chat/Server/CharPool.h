/*
 * MPool.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#ifndef CHARPOOL_H_
#define CHARPOOL_H_

#include <iostream>
#include <winsock2.h>
#include <concurrent_queue.h>
#include "common.h"

#define BLOCK_SIZE 1024

class CharPool {
private:
	char* data;
	Concurrency::concurrent_queue<char*> poolQueue;
	DWORD len;
	CharPool();
	~CharPool();
	static CharPool* instance; // Singleton Instance
public:
	// Singleton Instance 를 반환
	static CharPool* getInstance() {
		if (instance == nullptr) {
			cout << "char 메모리풀 2000개 할당!" << endl;
			instance = new CharPool();
		}
		return instance;
	}
	// 메모리풀 할당
	char* Malloc();
	// 메모리풀 반환
	void Free(char* freePoint);

};

#endif /* CHARPOOL_H_ */
