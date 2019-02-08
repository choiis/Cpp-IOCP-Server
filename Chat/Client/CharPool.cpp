/*
* MPool.cpp
*
*  Created on: 2019. 1. 17.
*      Author: choiis1207
*/

#include "CharPool.h"
// 생성자
CharPool::CharPool() {
	data = (char*)malloc(BLOCK_SIZE * 20000);
	DWORD i = 0;
	len = 20000;
	memset((char*)data, 0, BLOCK_SIZE * 20000);
	for (i = 0; i < 20000; i++) {
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

	if (poolQueue.empty()) { // 더 할당 필요한 경우 len(초기 blocks만큼 추가)
		char* nextP = new char[BLOCK_SIZE* len];
		cout << "CharPool More" << endl;
		memset((char*)nextP, 0, BLOCK_SIZE * len);
		for (DWORD j = 0; j < len; j++) {
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
	poolQueue.push((char*)freePoint);
}

