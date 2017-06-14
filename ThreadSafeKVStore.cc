#include <iostream>
#include <unordered_map>
#include <utility>
#include <pthread.h>
#include "ThreadSafeKVStore.h"

using namespace std;

template <class K, class V>
int ThreadSafeKVStore<K, V>::insert (const K key, const V value)
{
	pair <K, V> entry = make_pair(key, value); // Uses only stack variables, does not need to be locked
	
	pthread_rwlock_wrlock(&elementsLock);
	
	bool result = elements.insert(entry).second;
	
	if (!result)
	{
		auto result2 = elements.find(key);
		(*result2).second = value;
	}
	
	pthread_rwlock_unlock(&elementsLock);
	
	return 0;
}

template <class K, class V>
int ThreadSafeKVStore<K, V>::accumulate (const K key, const V value)
{
	pair <K, V> entry = make_pair(key, value); // Uses only stack variables, does not need to be locked
	
	pthread_rwlock_wrlock(&elementsLock);
	
	bool result = elements.emplace(entry).second;
	
	if (!result)
	{
		auto result2 = elements.find(key);
		(*result2).second = (*result2).second + value;
	}
	
	pthread_rwlock_unlock(&elementsLock);
	
	return 0;
}

template <class K, class V>
int ThreadSafeKVStore<K, V>::lookup (const K key, V& value)
{
	pthread_rwlock_rdlock(&elementsLock);
	
	auto result = elements.find(key);
		
	if (result != elements.end())
	{
		value = (*result).second;
		
		pthread_rwlock_unlock(&elementsLock);
		
		return 0;
	}
	else
	{
		pthread_rwlock_unlock(&elementsLock);
		
		return -1;
	}
}

template <class K, class V>
int ThreadSafeKVStore<K, V>::remove (const K key)
{	
	pthread_rwlock_wrlock(&elementsLock);
	
	int erased = elements.erase(key);
	
	pthread_rwlock_unlock(&elementsLock);
	
	return erased;
}

template <class K, class V>
void ThreadSafeKVStore<K, V>::init()
{
	pthread_rwlock_init(&elementsLock, NULL);
	pthread_mutex_init(&globalLock, NULL);
}

template <class K, class V>
int ThreadSafeKVStore<K, V>::size ()
{
	pthread_rwlock_rdlock(&elementsLock);
	
	int ret = elements.size();
	
	pthread_rwlock_unlock(&elementsLock);
	
	return ret;
}

template <class K, class V> // NOT THREAD SAFE, CHECK FOR COMPATIBLE TYPES
int ThreadSafeKVStore<K, V>::getSum ()
{
	int runningTotal = 0;
	
	for (auto iterator = elements.begin(); iterator != elements.end(); iterator++)
	{
		runningTotal += iterator->second; // Produce a running sum of all vlaues in KVStore
	}
	
	return runningTotal;
}

template <class K, class V> // CHECK FOR COMPATIBLE TYPES
void ThreadSafeKVStore<K, V>::getMin (K& val)
{
	int runningMin;
	K minVal;

	pthread_rwlock_wrlock(&elementsLock);
	
	for (auto iterator = elements.begin(); iterator != elements.end(); iterator++)
	{
		if (iterator == elements.begin())
			runningMin = iterator->second;
			
		if (iterator->second <= runningMin)
			minVal = iterator->first; // Get key associated with samllest value
	}
	
	val = minVal;
	
	pthread_rwlock_unlock(&elementsLock);
}
