#include "user.h"
#include "user_handler.h"

#include "mail.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <algorithm> 
#include <future>
#include <pthread.h>
#include <string_view>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#include <locale>

#include <nlohmann/json.hpp>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <openssl/sha.h>

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
bool ichar_equals(char a, char b);
bool iequals(std::string_view lhs, std::string_view rhs);

std::string saveToFile(fs::path object_dir, std::string message);
std::string getSha1(const std::string& p_arg);

// from myserver.c
void *clientCommunication(int data);
void signalHandler(int sig);

std::string cmdSEND(std::vector<std::string>& received);
std::string cmdLIST(std::vector<std::string>& received);
std::string cmdREAD(std::vector<std::string>& received);
std::string cmdDEL(std::vector<std::string>& received);

inline void exiting();
inline std::string read_file(std::string_view path);

int main (int argc, char* argv[])
{
	if (argc < 3 ||	
		!isInteger(argv[1])) {
		printUsage();
		return EXIT_FAILURE;
	}

	fs::path spool_dir = fs::path(argv[2]);

	if (fs::exists(spool_dir) && !fs::is_directory(spool_dir)) {
		printf("%s is not a directory\n", spool_dir.c_str());
		printUsage();
		return EXIT_FAILURE;
	}

	if (fs::create_directory(spool_dir)) {
		printf("%s does not exist, creating new...\n", spool_dir.c_str());
	}

	fs::create_directory(spool_dir/"users");
	fs::create_directory(spool_dir/"messages");

	user_handler::getInstance().setSpoolDir(spool_dir);

	std::atexit(exiting);

	char* p;
	uint16_t PORT = strtoul(argv[1], &p, 10);

	/* Code largely from the client-server-sample git / myserver.c */

	socklen_t addrlen;
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
		// clientCommunication(&new_socket); // returnValue can be ignored
		std::thread th(clientCommunication, new_socket);
		th.detach();
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
	printf("Usage: <twmailer-server [port] [path spool_directory]>\n");
}

void *clientCommunication(int data)
{
	char buffer[BUF];
	int size;
	int *current_socket = &data;

	std::string incomplete_message = "";

	do {
		std::string response;

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

		std::stringstream ss(incomplete_message.append(buffer)); //append instead of adding more lines to fix potentially split lines
		std::string line;

		std::vector<std::string> lines;
		while (std::getline(ss, line, '\n')) {
			lines.push_back(line);
		}


		enum commands cmd;

		// can't wait for reflections (maybe c++26?)
		if (iequals(lines.at(0), "SEND")) cmd = SEND;
		else if (iequals(lines.at(0), "LIST")) cmd = LIST;
		else if (iequals(lines.at(0), "READ")) cmd = READ;
		else if (iequals(lines.at(0), "DEL")) cmd = DEL;
		else if (iequals(lines.at(0), "QUIT")) break;
		else continue; // TODO: error message

		switch (cmd) {
			case SEND:
				if (lines.size() < 5 || lines.back().compare(".") != 0) {
					incomplete_message = buffer;
					continue; // issues if command end is never received
				}

				response = cmdSEND(lines);
				break;
			case LIST:
				response = cmdLIST(lines);
				break;
			case READ:
				response = cmdREAD(lines);
				break;
			case DEL:
				response = cmdDEL(lines);
				break;
			case QUIT:
				break;
			}

		if (send(*current_socket, response.c_str(), response.size(), 0) == -1) {
			perror("send answer failed");
			return NULL;
		}

		incomplete_message.clear();

	} while (!abortRequested);

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

std::string saveToFile(fs::path object_dir, std::string message)
{
	std::string sha1 = getSha1(message);
	std::ofstream ofs(object_dir/sha1); // possible issues with path length or file length limitations
	ofs << message;
	return sha1;
}

std::string getSha1(const std::string& str)
{
	unsigned char v[str.length() + 1];
	unsigned char hash[40];
	std::copy(str.data(), str.data() + str.length() + 1, v);

	SHA1(v, strlen((char *)v), hash);

    std::string out;

    for (int i = 0; i < 20; i++)
    {
    	char buf[3];
        std::sprintf(buf, "%02x", hash[i]);
        out.append(buf);
    }

    return out;
}

inline void exiting()
{
	user_handler::getInstance().saveAll();
	printf("Saving...	\n");
}

std::string cmdSEND(std::vector<std::string>& received)
{
	if (received.at(3).length() > 80)
		return "ERR\n";

	if (received.size() > 5) {
		for (std::vector<std::string>::iterator it = received.begin() + 5; it != received.end() && *it != "."; it++) {
			received.at(4).append("\n").append(*it);
		}
	}

	user_handler::getInstance().getOrCreateUser(received.at(1))->sendMail(
		new struct mail(saveToFile(user_handler::getInstance().getSpoolDir()/"messages", received.at(4)), received.at(3)),
		{received.at(2)}
	);

	return "OK\n"; // TODO: error handling
}

std::string cmdLIST(std::vector<std::string>& received)
{
	maillist inbox;
	user* user;
	if (received.size() < 2 || (user = user_handler::getInstance().getUser(received.at(1))) == nullptr)
		return "0\n";
		
	inbox = user->getMails();
	std::string response = std::to_string(std::count_if(inbox.begin(), inbox.end(), [](auto& mail) { return !mail->deleted; })) + "\n";
	for ( auto mail : inbox ) {
		if (mail->deleted)
			continue;
		response.append(std::to_string(mail->id)).append(": ").append(mail->subject).append("\n");
	}

	return response;
}

std::string cmdREAD(std::vector<std::string>& received)
{
	std::string response = "OK\n";

	user* user;
	mail* mail;

	char* p;

	if(received.size() < 3 ||
		!isInteger(received.at(2)) ||
		(user = user_handler::getInstance().getUser(received.at(1))) == nullptr ||
		(mail = user->getMail(strtoul(received.at(2).c_str(), &p, 10))) == nullptr ||
		mail->deleted)
		return "ERR\n";

	try {
		std::string path_str = user_handler::getInstance().getSpoolDir()/"messages"/mail->filename;
		response.append(read_file(path_str)).append("\n");
	}
	catch (...) { // TODO: more specific error handling - then again, it will respond with ERR either way
		return "ERR\n";
	}

	return response;
}

std::string cmdDEL(std::vector<std::string>& received)
{
	user* user;

	char* p;

	if(received.size() < 3 ||
		!isInteger(received.at(2)) ||
		(user = user_handler::getInstance().getUser(received.at(1))) == nullptr ||
		(user->delMail(strtoul(received.at(2).c_str(), &p, 10))) == false)
		return "ERR\n";

	return "OK\n";
}

// https://stackoverflow.com/a/116220
inline std::string read_file(std::string_view path)
{
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(path.data());
    stream.exceptions(std::ios_base::badbit);

   	if (not stream) {
   	    throw std::ios_base::failure("file does not exist");
   	}
    
    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(& buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    return out;
}

bool ichar_equals(char a, char b)
{
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

bool iequals(std::string_view lhs, std::string_view rhs)
{
    return std::ranges::equal(lhs, rhs, ichar_equals);
}