Main: Main.o ThreadSafeKVStore.o ThreadPoolServer.o
	g++ -pthread -Wall -std=c++11 -g ThreadSafeKVStore.o ThreadPoolServer.o Main.o -o Main

Main.o: Main.cc
	g++ -pthread -Wall -std=c++11 -g -c Main.cc

ThreadPoolServer.o: ThreadPoolServer.cc ThreadPoolServer.h
	g++ -pthread -Wall -std=c++11 -g -c ThreadPoolServer.cc

ThreadSafeKVStore.o: ThreadSafeKVStore.cc ThreadSafeKVStore.h
	g++ -pthread -Wall -std=c++11 -g -c ThreadSafeKVStore.cc

clean:
	rm *.o Main