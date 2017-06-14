#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>
#include <pthread.h>
#include <unistd.h>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <iterator>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "ThreadPoolServer.h"
#include "httpreq.cc"
#include "httpresp.cc"

using namespace std;

struct arguments // DEFINE ARGUMENTS CONSTRUCTOR-- SEE Main.cc FOR VARIABLE FUNCTIONS
{
	pthread_mutex_t m2; 
	pthread_mutex_t m; 
	ThreadSafeKVStore<string, string> kvStore;
	ThreadSafeKVStore<int, std::chrono::time_point<std::chrono::steady_clock>> timeStore;
	vector<double> medStore;
	void *listenerQ;
	double maxTime;
	double minTime;
	double avgTime;
	double medTime;
	int count;
	ThreadSafeKVStore<string, int> cacheStore;
	int cacheCount;
	pthread_mutex_t cacheLock;
};

static void error(const char *);

ThreadPoolServer::ThreadPoolServer(int threadTotal)
{
	threadCount = threadTotal;
	listenerQ.init(); // Initialize mutex and cond vars used by listenerQ 
}

void ThreadPoolServer::startThreads(void *args) // Pass in arguments and instructions to each thread
{
	void* threadFunction(void *);
	
	pthread_t threads[threadCount];
	
	for (int i = 0; i < threadCount; i++)
		pthread_create(&threads[i], NULL, threadFunction, (void *) args);
}

int ThreadPoolServer::startSocket()
{	
	int portno;
    socklen_t clilen;
    
    struct sockaddr_in serv_addr, cli_addr;
     
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
		error("ERROR opening socket");
        
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 3000; // Use static port number 3000
     
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);  
       
    if (::bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		error("ERROR on binding");
              
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    
    while (true) // Keep the server running indefinitely and push any accepted file descriptors to the listener queue
		listenerQ.push(accept(sockfd, (struct sockaddr *) &cli_addr, &clilen));
	
	return 0; 
}

void* threadFunction(void *ptr)
{	
											/* GET SHARED ARGUMENTS */
	
	struct arguments *args = (struct arguments *) ptr;
	ThreadSafeListenerQueue<int> *sharedQ = (ThreadSafeListenerQueue<int> *) args->listenerQ;
	
	std::chrono::time_point<std::chrono::steady_clock> startTime; // Initialize request time variable
	
	while (true) // Threads run indefinitely
	{		
									/* INSTANTIATE LOCAL THREAD VARIABLES */
		
		string numProcs = ""; // Number of HTTP method calls grabbed from kvstore
		int numProcsTwo = 0; // Converted HTTP method call number to int so it can be incremented
		
		string res = ""; // HTTP Response
		string meth = ""; // Request Method
		string bod = ""; // Request Body
		string val = ""; // Request Key
		
		int isThere = -1; // Return value of store lookups
		int req = 0; // Request file descriptor integer
		bool alive = true; // Request is keep alive by default
		bool first = true; // Thread has not yet tracked request times
		
										/* HANDLE INCOMING REQUEST */
			
		sharedQ->listen(req); // Listen for a request
		
		if (args->timeStore.lookup(req, startTime) == 0) // If request start time exists, get it
			first = false;
		
		HTTPReq request = HTTPReq(req);
		request.parse();
		
		if (request.isMalformed()) 
		{
			close(req);
			continue; // Close any malformed requests, restart loop
		}
		
		meth = request.getMethod();
		alive = request.getAlive();

		if (meth.compare("GET") == 0)
		{	
									/* PRINT AMOUNT OF GET OPERATIONS */
			
			pthread_mutex_lock(&(args->m));
			isThere = args->kvStore.lookup("GET", numProcs); // Checks whether or not the GET key exists
			if (isThere == 0)
			{
				numProcsTwo = stoi(numProcs);
				numProcsTwo++;
				numProcs = to_string(numProcsTwo);
				args->kvStore.insert("GET", numProcs); // If key exists, increment value and insert
			}
			else
			{
				args->kvStore.insert("GET", "1"); // If not, insert first GET
			}
			
			cout << "GET " << numProcs << "\n";
			pthread_mutex_unlock(&(args->m));
			
									/* CHECK IF KEY EXISTS IN KVSTORE */
			
			val = request.getURI();
			isThere = args->kvStore.lookup(val, bod);
			
			if (isThere == 0)
			{
				HTTPResp response = HTTPResp(200, val, alive); // Send 200 Response if key exists
				res = response.getResponse();
				
											/* UPDATE CACHE */
				
				args->cacheStore.insert(val, args->cacheCount);
				
				pthread_mutex_lock(&(args->cacheLock));
				args->cacheCount++;
				pthread_mutex_unlock(&(args->cacheLock));
				
									/* OUTPUT SERVER REQUEST DATA FOR GET 200 */
				
				if (!first)
				{
					auto end = std::chrono::steady_clock::now(); // Mark endtime of request
					std::chrono::duration<double> functionTime = end - startTime;
					double duration = functionTime.count() * 1000;
				
					pthread_mutex_lock(&(args->m2));
				
					args->medStore.push_back(duration); // Push new value to vector and grab median
					std::nth_element(args->medStore.begin(), args->medStore.begin() + args->medStore.size()/2, args->medStore.end());
					args->medTime = args->medStore[args->medStore.size()/2];
				
					args->count++; // Increment total operations
					
					if (duration < args->minTime)
						args->minTime = duration; // Set min
					if (duration > args->maxTime)
						args->maxTime = duration; // Set max
						
					args->avgTime = (args->avgTime * ((args->count)-1)/(args->count)) + (duration/(args->count)); // Running average
				
					cout << "\nMinimum: " << args->minTime << ", Maximum: " << args->maxTime << ", Average: " << args->avgTime << ", Median: " << args->medTime << "\n";
					cout << "Cache Size: ";
					cout << args->cacheStore.size();
					cout << "\n";
					pthread_mutex_unlock(&(args->m2));
				}
					
				send(req, res.c_str(), res.length(), 0); // Respond to client
			}
			else
			{
													/* CHECK DRIVE */

				string fileName = "store/" + val + ".txt";
				ifstream infile(fileName.c_str());
				
				if (infile.good()) // If file is present
				{
					
													/* UPDATE CACHE */
					
					HTTPResp response = HTTPResp(200, "", alive); // Send 200 Response if key exists
					res = response.getResponse();	
					
					if (args->cacheStore.size() >= 128) // If cache is at max size, remove LRU element
					{
						string eject;
						args->cacheStore.getMin(eject);
						args->cacheStore.remove(eject);
						args->kvStore.remove(eject);
					}
					
					args->cacheStore.insert(val, args->cacheCount);
					args->kvStore.insert(val, "");
				
					pthread_mutex_lock(&(args->cacheLock));
					args->cacheCount++;
					pthread_mutex_unlock(&(args->cacheLock));
					
				}
				else
				{
					HTTPResp response = HTTPResp(404, "", alive); // Send 404 Response if key does not exist
					res = response.getResponse();	
				}	
				
									/* OUTPUT SERVER REQUEST DATA FOR DISK GET */
				
				if (!first)
				{
					auto end = std::chrono::steady_clock::now(); // Mark endtime of request
					std::chrono::duration<double> functionTime = end - startTime;
					double duration = functionTime.count() * 1000;
				
					pthread_mutex_lock(&(args->m2));
				
					args->medStore.push_back(duration); // Push new value to vector and grab median
					std::nth_element(args->medStore.begin(), args->medStore.begin() + args->medStore.size()/2, args->medStore.end());
					args->medTime = args->medStore[args->medStore.size()/2];
				
					args->count++; // Increment total operations
					
					if (duration < args->minTime)
						args->minTime = duration; // Set min
					if (duration > args->maxTime)
						args->maxTime = duration; // Set max
						
					args->avgTime = (args->avgTime * ((args->count)-1)/(args->count)) + (duration/(args->count)); // Running average
				
					cout << "\nMinimum: " << args->minTime << ", Maximum: " << args->maxTime << ", Average: " << args->avgTime << ", Median: " << args->medTime << "\n";
					cout << "Cache Size: ";
					cout << args->cacheStore.size();
					cout << "\n";
					pthread_mutex_unlock(&(args->m2));
				}
				
				send(req, res.c_str(), res.length(), 0); // Respond to client
			}
		}
		
		else if (meth.compare("POST") == 0)
		{
									/* PRINT AMOUNT OF POST OPERATIONS */
			
			pthread_mutex_lock(&(args->m));
			isThere = args->kvStore.lookup("POST", numProcs); // Checks whether or not the POST key exists
			if (isThere == 0)
			{
				numProcsTwo = stoi(numProcs);
				numProcsTwo++;
				numProcs = to_string(numProcsTwo);
				args->kvStore.insert("POST", numProcs); // If key exists, increment value and insert
			}
			else
			{
				args->kvStore.insert("POST", "1"); // If not, insert first POST
			}
			
			cout << "POST " << numProcs << "\n";
			pthread_mutex_unlock(&(args->m));
			
			val = request.getURI();
			bod = request.getBody();
			
												/* FILE STORE */
			
			ofstream myFile;
			string fileName = "store/" + val + ".txt";
			myFile.open(fileName.c_str());
			myFile << val << "\n" << bod;
			myFile.close();
			
			HTTPResp response = HTTPResp(200, "", alive);
			res = response.getResponse();
			
									/* OUTPUT SERVER REQUEST DATA FOR POST 200 */
			
			if (!first)
			{
				auto end = std::chrono::steady_clock::now(); // Mark endtime of request
				std::chrono::duration<double> functionTime = end - startTime;
				double duration = functionTime.count() * 1000;
			
				pthread_mutex_lock(&(args->m2));
				
				args->medStore.push_back(duration); // Push new value to vector and grab median
				std::nth_element(args->medStore.begin(), args->medStore.begin() + args->medStore.size()/2, args->medStore.end());
				args->medTime = args->medStore[args->medStore.size()/2];
				
				args->count++; // Increment total operations
				
				if (duration < args->minTime)
					args->minTime = duration; // Set min
				if (duration > args->maxTime)
					args->maxTime = duration; // Set max
					
				args->avgTime = (args->avgTime * ((args->count)-1)/(args->count)) + (duration/(args->count)); // Running average
				
				cout << "\nMinimum: " << args->minTime << ", Maximum: " << args->maxTime << ", Average: " << args->avgTime << ", Median: " << args->medTime << "\n";
				cout << "Cache Size: ";
				cout << args->cacheStore.size();
				cout << "\n";
				pthread_mutex_unlock(&(args->m2));
			}
				
			send(req, res.c_str(), res.length(), 0); // Respond to client
		}
		
		else if (meth.compare("DELETE") == 0)
		{
			
										/* PRINT AMOUNT OF DELETE OPERATIONS */
			
			pthread_mutex_lock(&(args->m));
			isThere = args->kvStore.lookup("DELETE", numProcs); // Checks whether or not the DELETE key exists
			if (isThere == 0)
			{
				numProcsTwo = stoi(numProcs);
				numProcsTwo++;
				numProcs = to_string(numProcsTwo);
				args->kvStore.insert("DELETE", numProcs); // If key exists, increment value and insert
			}
			else
			{
				args->kvStore.insert("DELETE", "1"); // If not, insert first DELETE
			}
			cout << "DELETE " << numProcs << "\n";
			pthread_mutex_unlock(&(args->m));
			
			val = request.getURI();
			args->cacheStore.remove(val);
			isThere = args->kvStore.remove(val);
			
			if (isThere == 1)
			{
				HTTPResp response = HTTPResp(200, "", alive); // Send 200 Response if key exists
				res = response.getResponse();
				
											/* FILE STORE */
			
				string fileName = "store/" + val + ".txt";
				remove(fileName.c_str());
				
									/* OUTPUT SERVER REQUEST DATA FOR DELETE 200 */
				
				if (!first)
				{
					auto end = std::chrono::steady_clock::now(); // Mark endtime of thread
					std::chrono::duration<double> functionTime = end - startTime;
					double duration = functionTime.count() * 1000;
				
					pthread_mutex_lock(&(args->m2));
				
					args->medStore.push_back(duration); // Push new value to vector and grab median
					std::nth_element(args->medStore.begin(), args->medStore.begin() + args->medStore.size()/2, args->medStore.end());
					args->medTime = args->medStore[args->medStore.size()/2];
				
					args->count++; // Increment total operations
					
					if (duration < args->minTime)
						args->minTime = duration; // Set min
					if (duration > args->maxTime)
						args->maxTime = duration; // Set max
						
					args->avgTime = (args->avgTime * ((args->count)-1)/(args->count)) + (duration/(args->count)); // Running average
				
					cout << "\nMinimum: " << args->minTime << ", Maximum: " << args->maxTime << ", Average: " << args->avgTime << ", Median: " << args->medTime << "\n";
					cout << "Cache Size: ";
					cout << args->cacheStore.size();
					cout << "\n";
					pthread_mutex_unlock(&(args->m2));
				}
						
				send(req, res.c_str(), res.length(), 0); // Respond to client
			}
			else
			{
				
				HTTPResp response = HTTPResp(404, "", alive); // Send 404 Response if key does not exist
				res = response.getResponse();
				
												/* FILE STORE */
			
				string fileName = "store/" + val + ".txt";
				remove(fileName.c_str());
				
									/* OUTPUT SERVER REQUEST DATA FOR DELETE 404 */
				
				if (!first)
				{
					auto end = std::chrono::steady_clock::now(); // Mark endtime of thread
					std::chrono::duration<double> functionTime = end - startTime;
					double duration = functionTime.count() * 1000;
				
					pthread_mutex_lock(&(args->m2));
				
					args->medStore.push_back(duration); // Push new value to vector and grab median
					std::nth_element(args->medStore.begin(), args->medStore.begin() + args->medStore.size()/2, args->medStore.end());
					args->medTime = args->medStore[args->medStore.size()/2];
				
					args->count++; // Increment total operations
					
					if (duration < args->minTime)
						args->minTime = duration; // Set min
					if (duration > args->maxTime)
						args->maxTime = duration; // Set max
						
					args->avgTime = (args->avgTime * ((args->count)-1)/(args->count)) + (duration/(args->count)); // Running average
				
					cout << "\nMinimum: " << args->minTime << ", Maximum: " << args->maxTime << ", Average: " << args->avgTime << ", Median: " << args->medTime << "\n";
					cout << "Cache Size: ";
					cout << args->cacheStore.size();
					cout << "\n";
					pthread_mutex_unlock(&(args->m2));
				}
				
				send(req, res.c_str(), res.length(), 0); // Respond to client
			}
		}
		
		else
		{
			HTTPResp response = HTTPResp(400, "", alive); // IF NO VALID METHOD, SEND 400
			res = response.getResponse();		
			
								/* OUTPUT SERVER REQUEST DATA FOR INVALID METHOD */
			
			if (!first)
			{
				auto end = std::chrono::steady_clock::now(); // Mark endtime of thread
				std::chrono::duration<double> functionTime = end - startTime;
				double duration = functionTime.count() * 1000;
				
				pthread_mutex_lock(&(args->m2));
				
				args->medStore.push_back(duration); // Push new value to vector and grab median
				std::nth_element(args->medStore.begin(), args->medStore.begin() + args->medStore.size()/2, args->medStore.end());
				args->medTime = args->medStore[args->medStore.size()/2];
				
				args->count++; // Increment total operations
				
				if (duration < args->minTime)
					args->minTime = duration; // Set min
				if (duration > args->maxTime)
					args->maxTime = duration; // Set max
					
				args->avgTime = (args->avgTime * ((args->count)-1)/(args->count)) + (duration/(args->count)); // Running average
				
				cout << "\nMinimum: " << args->minTime << ", Maximum: " << args->maxTime << ", Average: " << args->avgTime << ", Median: " << args->medTime << "\n";
				cout << "Cache Size: ";
				cout << args->cacheStore.size();
				cout << "\n";
				pthread_mutex_unlock(&(args->m2));
			}
			
			send(req, res.c_str(), res.length(), 0); // Respond to client
		}
		
		if (alive) // If Keep-Alive, push connection onto listenerQ
		{
			sharedQ->push(req);
			args->timeStore.insert(req, std::chrono::steady_clock::now()); // Start request time
		}
		else // Otherwise close the connection
			close(req);
	}
}

void error(const char *msg)
{
    perror(msg);
    exit(1);
}
