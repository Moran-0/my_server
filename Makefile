src = $(wildcard src/*.cpp)

server:
	g++ -std=c++17 -pthread  -g \
	$(src) \
	server.cpp \
	-o server

client:
	g++ src/Buffer.cpp src/Util.cpp  src/Socket.cpp src/InetAddress.cpp client.cpp -o client

th:
	g++ -pthread src/ThreadPool.cpp test/ThreadPoolTest.cpp -o ThreadPoolTest

mytest:
	g++ src/Util.cpp src/Buffer.cpp src/Socket.cpp src/InetAddress.cpp src/ThreadPool.cpp \
	-pthread \
	-g \
	test/test.cpp -o mytest

clean:
	rm -f server client mytest ThreadPoolTest