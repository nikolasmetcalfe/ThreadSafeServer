#include <iostream>
#include <list>
#include <utility>
#include <pthread.h>
#include "ThreadSafeListenerQueue.h"

using namespace std;

template <class T>
int ThreadSafeListenerQueue<T>::push (const T element)
{
	pthread_mutex_lock(&queueLock);
	
	queue.push_front(element);
	empty = false;
	
	pthread_cond_signal(&cond); // Tell blocked threads that queue is no longer empty
	pthread_mutex_unlock(&queueLock);
	return 0;
}

template <class T>
int ThreadSafeListenerQueue<T>::pop (T& element)
{
	pthread_mutex_lock(&queueLock);
	
	if (queue.size() == 0)
	{
		pthread_mutex_unlock(&queueLock);
		return 1;
	}
	else
	{
		element = queue.back();
		queue.pop_back();
		
		if (queue.size() == 0)
			empty = true;
	
		pthread_mutex_unlock(&queueLock);
		return 0;
	}
}

template <class T>
int ThreadSafeListenerQueue<T>::listen (T& element)
{
	pthread_mutex_lock(&condLock); 
	
	while (empty)
		pthread_cond_wait(&cond, &condLock); // Lock if empty and wait for signal from push

	pthread_mutex_lock(&queueLock);
	element = queue.back();
	queue.pop_back();
	
	if (queue.size() == 0)
		empty = true;

	pthread_mutex_unlock(&condLock);
	pthread_mutex_unlock(&queueLock);
	return 0;
}

template <class T>
void ThreadSafeListenerQueue<T>::init()
{
	pthread_mutex_init(&queueLock, NULL);
	pthread_mutex_init(&condLock, NULL);
	pthread_cond_init(&cond, NULL);
}

template <class T> // Return private queue size
int ThreadSafeListenerQueue<T>::size ()
{
	return queue.size();
}

template <class T> // Output queue elements
void ThreadSafeListenerQueue<T>::printQueue ()
{
	for (auto iterator = queue.begin(); iterator != queue.end(); iterator++)
		cout << *iterator << "\n";
}

template <class T> // Get max of two queue elements using ">" operator-- Check for compatible type T
T ThreadSafeListenerQueue<T>::getMax()
{
	T max = 0;
	pthread_mutex_lock(&queueLock);
	
	for (auto iterator = queue.begin(); iterator != queue.end(); iterator++)
	{
		if (*iterator > max)
			max = *iterator;
	}
	
	pthread_mutex_unlock(&queueLock);
	return max;
}
