/*
 * MPool.h
 *
 *  Created on: 2019. 1. 17.
 *      Author: choiis1207
 */

#ifndef MPOOL_H_
#define MPOOL_H_

#include <iostream>
#include <winsock2.h>
#include "common.h"

class MPool {
private:
	char* data; // 동적할당 대상 주소
	bool* arr; // block들의 할당 여부
	DWORD cnt; // 전체 블록수
	DWORD idx; // 반환 대상 idx
	MPool();
	~MPool();
	static MPool* instance; // Singleton Instance
public:
	// Singleton Instance 를 반환
	static MPool* getInstance() {
		if (instance == nullptr) {
			cout <<"메모리풀 1000개 할당!"<<endl;
			instance = new MPool();
		}
		return instance;
	}
	// 메모리풀 할당
	LPPER_IO_DATA malloc();
	// 메모리풀 반환
	void free(LPPER_IO_DATA freePoint);
	// 사이즈
	DWORD blockSize() {
		return cnt;
	}
};

#endif /* MPOOL_H_ */
