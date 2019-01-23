/*
 * MPool.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "CharPool.h"
// 생성자
CharPool::CharPool() {
	data = (char*) malloc(BLOCK_SIZE * 1000);
	DWORD i = 0;
	len = 1000;
	InitializeCriticalSectionAndSpinCount(&cs, 2000);
	for (i = 0; i < 1000; i++) {
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
		data = (char*) realloc(data, (len + len) * BLOCK_SIZE);
		DWORD i = 0;
		for (i = 0; i < len; i++) {
			poolQueue.push(data + (BLOCK_SIZE * (len + i)));
		}
		len += len;
	}
	char* pointer = poolQueue.front();
	poolQueue.pop();
	LeaveCriticalSection(&cs);
	return pointer;
}
// 메모리풀 반환
void CharPool::Free(char* freePoint) { // 반환한 포인터의 idx를 원상복구
	EnterCriticalSection(&cs);
	poolQueue.push((char*) freePoint);
	LeaveCriticalSection(&cs);
}

