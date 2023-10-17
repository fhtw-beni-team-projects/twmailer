#include "user.h"
#include "user_handler.h"

#include "readline.h"

#include <boost/algorithm/string/predicate.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/compute/detail/sha1.hpp>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <vector>

#define BUF 1024

enum commands {
	SEND = 1,
	LIST,
	READ,
	DEL,
	QUIT = -1
};

namespace fs = std::filesystem;

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

void printUsage();
inline bool isInteger(const std::string & s);

void saveToFile(fs::path object_dir, std::string message);
std::string get_sha1(const std::string& p_arg);

// from myserver.c
void *clientCommunication(void *data);
void signalHandler(int sig);

user_handler* user_handler::instancePtr = nullptr;

int main (int argc, char* argv[])
{
	if (argc < 3 ||	
		!isInteger(argv[1]) ||
		!fs::is_directory(argv[2])
		) {
		printUsage();
		return EXIT_FAILURE;
	}

	fs::path spool_dir;
	user_handler::getInstance()->setSpoolDir(spool_dir = fs::path(argv[2]));

	fs::create_directory(spool_dir/"users");
	fs::create_directory(spool_dir/"messages");

	char* p;
	u_long PORT = strtoul(argv[1], &p, 10);

	/* Code largely from the client-server-sample git / myserver.c */

	__socklen_t addrlen;
	struct sockaddr_in address, cliaddress;
	int reuseValue = 1;

	if (signal(SIGINT, signalHandler) == SIG_ERR) {
		perror("signal can not be registered");
		return EXIT_FAILURE;
	}

	if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket error"); // errno set by socket()
		return EXIT_FAILURE;
	}

	if (setsockopt(create_socket,
						SOL_SOCKET,
						SO_REUSEADDR,
						&reuseValue,
						sizeof(reuseValue)) == -1) {
		perror("set socket options - reuseAddr");
		return EXIT_FAILURE;
	}

	if (setsockopt(create_socket,
						SOL_SOCKET,
						SO_REUSEPORT,
						&reuseValue,
						sizeof(reuseValue)) == -1) {
		perror("set socket options - reusePort");
		return EXIT_FAILURE;
	}

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1) {
		perror("bind error");
		return EXIT_FAILURE;
	}

	if (listen(create_socket, 5) == -1) {
		perror("listen error");
		return EXIT_FAILURE;
	}

	while (!abortRequested) {
		printf("Waiting for connections...\n");

		addrlen = sizeof(struct sockaddr_in);
		if ((new_socket = accept(create_socket,
										 (struct sockaddr *)&cliaddress,
										 &addrlen)) == -1) {
			if (abortRequested) {
				perror("accept error after aborted");
			}
			else {
				perror("accept error");
			}
			break;
		}

		printf("Client connected from %s:%d...\n",
				 inet_ntoa(cliaddress.sin_addr),
				 ntohs(cliaddress.sin_port));
		clientCommunication(&new_socket); // returnValue can be ignored
		new_socket = -1;
	}


	if (create_socket != -1) {
		if (shutdown(create_socket, SHUT_RDWR) == -1) {
			perror("shutdown create_socket");
		}
		if (close(create_socket) == -1) {
			perror("close create_socket");
		}
		create_socket = -1;
	}

	return EXIT_SUCCESS;

}

// https://stackoverflow.com/questions/2844817/how-do-i-check-if-a-c-string-is-an-int
inline bool isInteger(const std::string & s)
{
	if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;

	char * p;
	strtol(s.c_str(), &p, 10);

	return (*p == 0);
}

void printUsage()
{
	printf("printUsage\n");
}

void *clientCommunication(void *data)
{
	char buffer[BUF];
	int size;
	int *current_socket = (int *)data;

	strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
	if (send(*current_socket, buffer, strlen(buffer), 0) == -1) {
		perror("send failed");
		return NULL;
	}

	do {
		size = recv(*current_socket, buffer, BUF - 1, 0);
		if (size == -1) {
			if (abortRequested) {
				perror("recv error after aborted");
			} else {
				perror("recv error");
			}
			break;
		}

		if (size == 0) {
			printf("Client closed remote socket\n"); // ignore error
			break;
		}

		// remove ugly debug message, because of the sent newline of client
		if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n') {
			size -= 2;
		} else if (buffer[size - 1] == '\n') {
			--size;
		}

		buffer[size] = '\0';




		/* New code here for handling requests */
		

		std::stringstream ss(buffer);
		std::string line;

		std::vector<std::string> message;

		while (std::getline(ss, line, '\n')) {
			message.push_back(line);
		}

		enum commands cmd;

		// can't wait for reflections (maybe c++26?)
		if (boost::iequals(message.at(0), "SEND")) cmd = SEND;
		else if (boost::iequals(message.at(0), "LIST")) cmd = LIST;
		else if (boost::iequals(message.at(0), "READ")) cmd = READ;
		else if (boost::iequals(message.at(0), "DEL")) cmd = DEL;
		else if (boost::iequals(message.at(0), "QUIT")) cmd = QUIT;

		switch (cmd) {
			case SEND:
			case LIST:
			case READ:
			case DEL:
			case QUIT:
				break;
			}

		if (send(*current_socket, "OK", 3, 0) == -1) {
			perror("send answer failed");
			return NULL;
		}
	} while (strcmp(buffer, "quit") != 0 && !abortRequested);

	// closes/frees the descriptor if not already
	if (*current_socket != -1) {
		if (shutdown(*current_socket, SHUT_RDWR) == -1) {
			perror("shutdown new_socket");
		}
		if (close(*current_socket) == -1) {
			perror("close new_socket");
		}
		*current_socket = -1;
	}

	return NULL;
}

void signalHandler(int sig)
{
	if (sig == SIGINT) {
		printf("abort Requested... "); // ignore error
		abortRequested = 1;
		if (new_socket != -1)
		{
			if (shutdown(new_socket, SHUT_RDWR) == -1)
			{
				perror("shutdown new_socket");
			}
			if (close(new_socket) == -1)
			{
				perror("close new_socket");
			}
			new_socket = -1;
		}

		if (create_socket != -1) {
			if (shutdown(create_socket, SHUT_RDWR) == -1) {
				perror("shutdown create_socket");
			}
			if (close(create_socket) == -1) {
				perror("close create_socket");
			}
			create_socket = -1;
		}
	} else {
		exit(sig);
	}
}

void saveToFile(fs::path object_dir, std::string message)
{
	std::string sha1 = get_sha1(message);
	std::ofstream ofs(object_dir/sha1); // possible issues with path length or file length limitations
	ofs << message;
}

// https://stackoverflow.com/questions/28489153/how-to-portably-compute-a-sha1-hash-in-c
std::string get_sha1(const std::string& p_arg)
{
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    // Back to string
    char buf[41] = {0};

    for (int i = 0; i < 5; i++)
    {
        std::sprintf(buf + (i << 3), "%08x", hash[i]);
    }

    return std::string(buf);
}