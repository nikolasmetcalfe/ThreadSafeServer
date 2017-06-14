#include <iostream>
#include <list>
#include <utility>
#include <limits>
#include <pthread.h>

using namespace std;

template <class T>
class ThreadSafeListenerQueue
{
	private:
		pthread_mutex_t queueLock;
		pthread_mutex_t condLock;
		pthread_cond_t cond;
		bool empty = true;
	
	public:
		list<T> queue;
		int push(const T);
		int pop(T&);
		int listen(T&);
		int size();
		T getMax();
		void printQueue();
		void init();
};
