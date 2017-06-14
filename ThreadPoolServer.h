#include <iostream>
#include <list>
#include <utility>
#include <pthread.h>
#include <list>
#include "ThreadSafeListenerQueue.cc"
#include "ThreadSafeKVStore.cc"

using namespace std;

class ThreadPoolServer
{
	private:
		int threadCount;
	
	public:
		ThreadSafeListenerQueue<int> listenerQ;
		int sockfd;
		
		ThreadPoolServer(int);
		void startThreads(void *args);
		int startSocket();
		static void *threadFunction(void *);
};
