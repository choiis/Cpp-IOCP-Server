/*
 * MPool.cpp
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#include "MPool.h"
// 생성자
MPool::MPool() {
	data = new char[1000 * sizeof(PER_IO_DATA)]; // size 곱하기 블록수 만큼 할당
	arr = new bool[1000]; // idx번째 메모리 풀의 할당 여부를 가진다
	cnt = 1000;
	idx = 0;
	InitializeCriticalSectionAndSpinCount(&cs, 2000);
	memset(this->arr, 0, 1000);
}
// Singleton Instance
MPool* MPool::instance = nullptr;

MPool::~MPool() {
	delete data;
	delete arr;
	DeleteCriticalSection(&cs);
}
// 메모리풀 할당
LPPER_IO_DATA MPool::malloc() {
	EnterCriticalSection(&cs);
	// 할당이 안된 저장소를 찾는다
	while (arr[idx]) {
		idx++;
		if (idx == cnt) {
			idx = 0;
		}
	}

	arr[idx] = true; // 할당여부 true
	idx++;
	char* alloc;
	if (idx == cnt) { // 인덱스가 마지막 번째일때
		idx = 0;
		alloc = (data + ((cnt - 1) * (sizeof(PER_IO_DATA))));
	} else {
		alloc = (data + ((idx - 1) * (sizeof(PER_IO_DATA))));
	}
	LeaveCriticalSection(&cs);
	return (LPPER_IO_DATA) alloc;
}
// 메모리풀 반환
void MPool::free(LPPER_IO_DATA freePoint) { // 반환한 포인터의 idx를 원상복구

	DWORD returnIdx = ((((char*) freePoint) - data) / sizeof(PER_IO_DATA));
	EnterCriticalSection(&cs);
	memset(freePoint, 0, sizeof(PER_IO_DATA));
	idx = returnIdx; // 반환된것을 바로 반환
	// 반환된 idx의 할당여부 No
	arr[returnIdx] = false;
	LeaveCriticalSection(&cs);
}

