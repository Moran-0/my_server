server:
	g++ src/Buffer.cpp src/Connection.cpp src/Acceptor.cpp src/EventLoop.cpp src/Channel.cpp src/Util.cpp src/Server.cpp src/Epoll.cpp src/InetAddress.cpp src/Socket.cpp server.cpp -o server
client:
	g++ src/Buffer.cpp src/Util.cpp  src/Socket.cpp src/InetAddress.cpp client.cpp -o client
clean:
	rm server && rm client