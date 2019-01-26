/*
 * MPool.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "CharPool.h"
// 생성자
CharPool::CharPool() {
	data = (char*)malloc(BLOCK_SIZE * 2000);
	DWORD i = 0;
	len = 2000;
	memset((char*)data, 0, BLOCK_SIZE* 2000);
	InitializeCriticalSectionAndSpinCount(&cs, 2000);
	for (i = 0; i < 2000; i++) {
		poolQueue.push(data + (BLOCK_SIZE * i));
	}
}
// Singleton Instance
CharPool* CharPool::instance = nullptr;

CharPool::~CharPool() {
	DeleteCriticalSection(&cs);
	while (!poolQueue.empty()) {
		poolQueue.pop();
	}
	free(data);
}
// 메모리풀 할당
char* CharPool::Malloc() {
	EnterCriticalSection(&cs);
	if (poolQueue.empty()) { // 더 할당 필요한 경우 len(초기 blocks만큼 추가)
		char* nextP = new char[BLOCK_SIZE* len];
		memset((char*)nextP, 0, BLOCK_SIZE * len);
		for (DWORD j = 0; j < len; j++) {
			poolQueue.push(nextP + (BLOCK_SIZE* j));
		}
	}
	char* pointer = poolQueue.front();
	poolQueue.pop();
	LeaveCriticalSection(&cs);
	return pointer;
}
// 메모리풀 반환
void CharPool::Free(char* freePoint) { // 반환한 포인터의 idx를 원상복구
	EnterCriticalSection(&cs);
	memset((char*)freePoint, 0, sizeof(BLOCK_SIZE));
	poolQueue.push((char*) freePoint);
	LeaveCriticalSection(&cs);
}

