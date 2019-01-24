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
#include <queue>
#include "common.h"

#define BLOCK_SIZE 512

class CharPool {
private:
	char* data;
	queue<char*> poolQueue;
	DWORD len;
	CRITICAL_SECTION cs; // 메모리풀 동기화 크리티컬섹션
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
