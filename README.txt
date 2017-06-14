Utilizing a backbone of simple thread safe data structures, I’ve created a thread pool based server that operates over the HTTP protocol (supports GET, POST, and DELETE operations) using an in-memory store as a cache in front of an on-disk store.

To start the server, enter the make command and run ./Main -n <threadCount>, where threadCount is a positive integer.

The server is bound to port number 3000.

The file “exz.txt” has been provided as input for the wsesslog parameter of HTTPerf for performance testing.