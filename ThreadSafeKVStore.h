#include <iostream>
#include <unordered_map>
#include <utility>
#include <pthread.h>

using namespace std;

template <class K, class V>
class ThreadSafeKVStore
{
	private:
		unordered_map<K, V> elements;
		pthread_rwlock_t elementsLock;
		pthread_mutex_t globalLock;
	
	public:
		int insert(const K, const V);
		int accumulate(const K, const V);
		int lookup(const K, V&);
		int remove(const K);
		int size();
		int getSum();
		void getMin(K&);
		void init();
};
