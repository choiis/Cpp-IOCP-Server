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

#define BLOCK_SIZE 256

class CharPool {
private:
	char* data; // 동적할당 대상 주소
	bool* arr; // block들의 할당 여부
	DWORD cnt; // 전체 블록수
	DWORD idx; // 반환 대상 idx
	CRITICAL_SECTION cs; // 메모리풀 동기화 크리티컬섹션
	CharPool();
	~CharPool();
	static CharPool* instance; // Singleton Instance
public:
	// Singleton Instance 를 반환
	static CharPool* getInstance() {
		if (instance == nullptr) {
			instance = new CharPool();
		}
		return instance;
	}
	// 메모리풀 할당
	char* malloc();
	// 메모리풀 반환
	void free(char* freePoint);
	// 사이즈
	DWORD blockSize() {
		return cnt;
	}
};

#endif /* CHARPOOL_H_ */
