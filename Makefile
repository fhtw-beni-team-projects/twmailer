all: client server

client: client.cpp
	g++ -std=c++17 -Wall -Werror -fsanitize=address -o twmailer-client client.cpp

server: server.cpp
	g++ -std=c++17 -Wall -Werror -fsanitize=address -o twmailer-server server.cpp

clean:
	rm -f twmailer-client twmailer-server