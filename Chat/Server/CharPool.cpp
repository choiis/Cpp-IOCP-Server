/*
 * MPool.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "CharPool.h"
// 생성자
CharPool::CharPool() {
	data = (char*)malloc(BLOCK_SIZE * 30000);
	DWORD i = 0;
	len = 30000;
	memset((char*)data, 0, BLOCK_SIZE * 30000);
	for (i = 0; i < 30000; i++) {
		poolQueue.push(data + (BLOCK_SIZE * i));
	}
}
// Singleton Instance
CharPool* CharPool::instance = nullptr;

CharPool::~CharPool() {
	char *popPoint;
	while (!poolQueue.empty()) {
		poolQueue.try_pop(popPoint);
	}
	free(data);
}
// 메모리풀 할당
char* CharPool::Malloc() {
	
	if (poolQueue.empty()) { // 더 할당 필요한 경우 2000만큼 추가
		char* nextP = new char[BLOCK_SIZE * 2000];
		cout << "CharPool More" << endl;
		memset((char*)nextP, 0, BLOCK_SIZE * 2000);
		for (DWORD j = 0; j < 2000; j++) {
			poolQueue.push(nextP + (BLOCK_SIZE* j));
		}
	}
	char* pointer;
	poolQueue.try_pop(pointer);
	
	return pointer;
}
// 메모리풀 반환
void CharPool::Free(char* freePoint) { // 반환한 포인터의 idx를 원상복구
	memset((char*)freePoint, 0, sizeof(BLOCK_SIZE));
	poolQueue.push((char*) freePoint);
}

