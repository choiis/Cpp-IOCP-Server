/*
 * MPool.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "CharPool.h"
// 생성자
CharPool::CharPool() {
	data = new char[1000 * BLOCK_SIZE]; // size 곱하기 블록수 만큼 할당
	arr = new bool[1000]; // idx번째 메모리 풀의 할당 여부를 가진다
	cnt = 1000;
	idx = 0;
	memset(this->arr, 0, 1000);
}
// Singleton Instance
CharPool* CharPool::instance = nullptr;

CharPool::~CharPool() {
	delete data;
	delete arr;
}
// 메모리풀 할당
char* CharPool::malloc() {
	// 할당이 안된 저장소를 찾는다
	while (arr[idx]) {
		idx++;
		if (idx == cnt) {
			idx = 0;
		}
	}

	arr[idx] = true; // 할당여부 true
	idx++;
	if (idx == cnt) { // 인덱스가 마지막 번째일때
		idx = 0;
		return (data + ((cnt - 1) * BLOCK_SIZE));
	} else {
		return (data + ((idx - 1) * BLOCK_SIZE));
	}

}
// 메모리풀 반환
void CharPool::free(char* freePoint) { // 반환한 포인터의 idx를 원상복구
	DWORD returnIdx = ((((char*) freePoint) - data) / BLOCK_SIZE);
	idx = returnIdx;
	// 반환된 idx의 할당여부 No
	arr[returnIdx] = false;
}

