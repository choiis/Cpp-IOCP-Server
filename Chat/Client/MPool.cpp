/*
 * MPool.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "MPool.h"
// 생성자
MPool::MPool() {
	data = (char*) malloc(sizeof(PER_IO_DATA) * 2000);
	DWORD i = 0;
	len = 2000;
	memset((char*)data, 0, sizeof(PER_IO_DATA)* 2000);
	InitializeCriticalSectionAndSpinCount(&cs, 2000);
	for (i = 0; i < 2000; i++) {
		poolQueue.push(data + (sizeof(PER_IO_DATA) * i));
	}
}
// Singleton Instance
MPool* MPool::instance = nullptr;

MPool::~MPool() {
	DeleteCriticalSection(&cs);
	while (!poolQueue.empty()) {
		poolQueue.pop();
	}
	free(data);
}
// 메모리풀 할당
LPPER_IO_DATA MPool::Malloc() {
	EnterCriticalSection(&cs);
	if (poolQueue.empty()) { // 더 할당 필요한 경우 len(초기 blocks만큼 추가)
		char* nextP = data + (sizeof(PER_IO_DATA)* len);
		nextP = new char[sizeof(PER_IO_DATA)* len];
		memset((char*)nextP, 0, sizeof(PER_IO_DATA)* len);
		for (DWORD j = 0; j < len; j++) {
			poolQueue.push(nextP + (sizeof(PER_IO_DATA)* j));
		}
	}
	LPPER_IO_DATA pointer = (LPPER_IO_DATA) poolQueue.front();
	poolQueue.pop();
	LeaveCriticalSection(&cs);
	return pointer;
}
// 메모리풀 반환
void MPool::Free(LPPER_IO_DATA freePoint) { // 반환한 포인터의 idx를 원상복구
	EnterCriticalSection(&cs);
	poolQueue.push((char*) freePoint);
	LeaveCriticalSection(&cs);
}

