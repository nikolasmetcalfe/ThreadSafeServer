#include <pthread.h>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <random>
#include <ctime>
#include <chrono>
#include <vector>
#include <thread>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include "ThreadPoolServer.h"

using namespace std;

struct arguments
{
	pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER; // Lock for displaying server side data output
	pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; // Lock for displaying operation counts
	ThreadSafeKVStore<string, string> kvStore; // Key-Value Store
	ThreadSafeKVStore<int, std::chrono::time_point<std::chrono::steady_clock>> timeStore; // Request-Time Store
	vector<double> medStore; // Vector for calculating median
	void *listenerQ; // ThreadPoolServer listener queue
	int count = 0; // Keeps track of total requests
	double maxTime = 0; // Max request time
	double minTime = 10000; // Min request time
	double avgTime = 0; // Average request time
	double medTime = 0; // Median request time
	ThreadSafeKVStore<string, int> cacheStore; // Pairs each cache variable with last use
	int cacheCount = 0; // Keep track of last key use
	pthread_mutex_t cacheLock = PTHREAD_MUTEX_INITIALIZER; // Lock changes to cacheCount
};

int main(int argc, char **argv)
{
	
										/* READ COMMANDLINE INPUT */
										
	int nflag = 0;
	int threadNum = 0;
	char* nvalue = NULL;
	int c;
	
	if ((c = getopt(argc, argv, "n:")) != -1) 
	{
		nflag = 1;
		nvalue = optarg;
	}
	else
	{
		cout << "Error: Must provide -n <N> positive, non-zero integer as command line argument\n";
		return 0;
	}
	
	threadNum = stoi(nvalue, nullptr, 0); // Set thread count to number passed by -n
	
	if (nflag == 0 || threadNum <= 0) // Return error if there is no argument or the argument creates no threads
	{
		cout << "Error: Must provide -n <N> positive, non-zero integer as command line argument\n";
		return 0;
	}
	
										/* INSTANTIATE THREADPOOL */
	
	arguments *args = new arguments; // Create variables that will be passed to threads
	
	// Initialize mutex and cond vars for all data structures
	args->kvStore.init();
	args->timeStore.init();
	args->cacheStore.init();
	
	ThreadPoolServer *threadPool = new ThreadPoolServer(threadNum);
	args->listenerQ = &threadPool->listenerQ; // Set the listenerQ passed to threads to the queue used by the server
	
	threadPool->startThreads(args); // Initialize threads in threadpool
	threadPool->startSocket(); // Run server on infinite loop
	
	return 0;
}	
