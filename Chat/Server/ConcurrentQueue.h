
#ifndef CONCURRENTQUEUE_H_
#define CONCURRENTQUEUE_H_

#include <queue>
#include <mutex>

using namespace std;

template<typename T>
class ConcurrentQueue : private queue<T> {
private:
	mutex lock;
public:
	void push(const T& t) {
		lock_guard<mutex> guard(lock);
		queue<T>::push(t);
	}

	T& top() {
		T t;
		{
			lock_guard<mutex> guard(lock);
			t = queue<T>::front();
			queue<T>::pop();
		}
		return t;
	}

	bool empty() {
		lock_guard<mutex> guard(lock);
		return queue<T>::empty();
	}

	int size() {
		lock_guard<mutex> guard(lock);
		return queue<T>::size();
	}
};
#endif /* CONCURRENTQUEUE_H_ */
