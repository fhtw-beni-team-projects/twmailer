#include "ip_ban.h"
#include "user.h"
#include "user_handler.h"

#include "mail.h"


#include <ldap.h>


#include <algorithm>
#include <ranges>
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
#include <mutex>
#include <pthread.h>
#include <string_view>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <locale>

#include <nlohmann/json.hpp>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF 1024

enum commands {
	LOGIN = 1,
	SEND,
	LIST,
	READ,
	DEL,
	QUIT = -1
};

namespace fs = std::filesystem;

bool abortRequested = false;
int create_socket = -1;
int new_socket = -1;

std::vector<pthread_t> threads;

void printUsage();
inline bool isInteger(const std::string & s);
bool ichar_equals(char a, char b);
bool iequals(std::string_view lhs, std::string_view rhs);

std::string saveToFile(fs::path object_dir, std::string message);
std::string getSha1(const std::string& p_arg);

// from myserver.c
void *clientCommunication(void *data);
void signalHandler(int sig);

std::string cmdLOGIN(std::vector<std::string>& received, std::string& loggedInUsername, const std::string& ip);
std::string cmdSEND(std::vector<std::string>& received, const std::string& loggedInUsername);
std::string cmdLIST(std::vector<std::string>& received, const std::string& loggedInUsername);
std::string cmdREAD(std::vector<std::string>& received, const std::string& loggedInUsername);
std::string cmdDEL(std::vector<std::string>& received, const std::string& loggedInUsername);


inline void exiting();
inline std::string read_file(std::string_view path);

struct args
{
    int socket;
    std::string ip;
    fs::path spool_dir;
};

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

	ip_ban::getInstance().loadFile(spool_dir / "bans.json");

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
		pthread_t tid;
		pthread_create(&tid, NULL, clientCommunication, static_cast<void *>(new args{new_socket, inet_ntoa(cliaddress.sin_addr), spool_dir}));
		threads.push_back(tid);
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

void *clientCommunication(void *data)
{
	args* args = (struct args*) data;

	char buffer[BUF];
	int size;
	int *current_socket = &args->socket;
	std::string ip = args->ip;

	std::string incomplete_message = "";

	std::string loggedInUsername;

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
		if (iequals(lines.at(0), "LOGIN")) cmd = LOGIN;
		else if (iequals(lines.at(0), "SEND")) cmd = SEND;
		else if (iequals(lines.at(0), "LIST")) cmd = LIST;
		else if (iequals(lines.at(0), "READ")) cmd = READ;
		else if (iequals(lines.at(0), "DEL")) cmd = DEL;
		else if (iequals(lines.at(0), "QUIT")) break;
		else continue; // TODO: error message

		switch (cmd) {
		case LOGIN:
			response = cmdLOGIN(lines, loggedInUsername, ip);
			break;
		case SEND:
			if (lines.size() < 5 || lines.back().compare(".") != 0) {
            	incomplete_message = buffer;
            	continue; 
        		}
        	response = cmdSEND(lines, loggedInUsername);
        	break;
		case LIST:
			response = cmdLIST(lines, loggedInUsername);
			break;
		case READ:
			response = cmdREAD(lines, loggedInUsername);
			break;
		case DEL:
			response = cmdDEL(lines, loggedInUsername);
			break;
		case QUIT:
			break;
		default:
			// invalid command
			response = "ERR\n";
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

	delete(args);
	return NULL;
}

void signalHandler(int sig)
{
	if (sig == SIGINT) {
		printf("abort Requested... "); // ignore error
		abortRequested = true;
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
    for (auto& thread : threads) {
    	pthread_join(thread, NULL);
    }

	user_handler::getInstance().saveAll();
	printf("Saving...	\n");
}

std::string cmdLOGIN(std::vector<std::string>& received, std::string& loggedInUsername, const std::string& ip)
{
	if (received.size() < 3) {
        return "ERR\n";
    }

	const char* ldapUri = "ldap://ldap.technikum-wien.at:389";
    const int ldapVersion = LDAP_VERSION3;
    LDAP* ldapHandle;
    int rc = ldap_initialize(&ldapHandle, ldapUri);
    if (rc != LDAP_SUCCESS) {
        return "ERR\n";
    }

	rc = ldap_set_option(ldapHandle, LDAP_OPT_PROTOCOL_VERSION, &ldapVersion);
    if (rc != LDAP_OPT_SUCCESS) {
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        return "ERR\n"; 
    }

	rc = ldap_start_tls_s(ldapHandle, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        return "ERR\n";
    }

	std::string ldapBindUser = "uid=" + received.at(1) + ",ou=people,dc=technikum-wien,dc=at";
    std::string ldapBindPassword = received.at(2);

	BerValue bindCredentials;
    bindCredentials.bv_val = (char*) ldapBindPassword.c_str();
    bindCredentials.bv_len = ldapBindPassword.length();
    rc = ldap_sasl_bind_s(ldapHandle, ldapBindUser.c_str(), LDAP_SASL_SIMPLE, &bindCredentials, NULL, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
    	ip_ban::getInstance().failedAttempt(received.at(1), ip);
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        return "ERR\n";
    }

    if (ip_ban::getInstance().checkBanned(ip)) {
		ldap_unbind_ext_s(ldapHandle, NULL, NULL);
		return "ERR\n";
    }

    loggedInUsername = received.at(1);
    ip_ban::getInstance().success(ip);
	ldap_unbind_ext_s(ldapHandle, NULL, NULL);
	
	return "OK\n";
}	


std::string cmdSEND(std::vector<std::string>& received, const std::string& loggedInUsername) {

    if (loggedInUsername.empty() || received.size() < 4) {
        return "ERR\n";
    }

	std::string receiver = received.at(1);
    std::string subject = received.at(2);
    std::string message = received.at(3);

	if (subject.length() > 80) {
        return "ERR\n";
    }

	user_handler::getInstance().getOrCreateUser(receiver)->addMail(
        new struct mail(saveToFile(user_handler::getInstance().getSpoolDir()/"messages", message), subject, loggedInUsername)
    );

	return "OK\n"; // TODO: error handling

}

std::string cmdLIST(std::vector<std::string>& received, const std::string& loggedInUsername) {
    if (loggedInUsername.empty()) {
    	printf("%s %zu\n", loggedInUsername.c_str(), received.size());
        return "ERR\n";
    }

    user* currentUser = user_handler::getInstance().getUser(loggedInUsername);
    if (currentUser == nullptr) {
        return "0\n";
    }

    maillist inbox = currentUser->getMails();
    std::string response = std::to_string(std::count_if(inbox.begin(), inbox.end(), [](auto& mail) { return !mail->deleted; })) + "\n";
    for (auto& mail : inbox) {
        if (!mail->deleted) {
            response.append(std::to_string(mail->id)).append(": ").append(mail->subject).append("\n");
        }
    }

    return response;
}

std::string cmdREAD(std::vector<std::string>& received, const std::string& loggedInUsername) {
    if (loggedInUsername.empty() || received.size() < 2) {
    	printf("%s %zu\n", loggedInUsername.c_str(), received.size());
        return "ERR\n";
    }

    char* p;
    long mailId = strtol(received.at(1).c_str(), &p, 10);
    if (*p != 0) { 
        return "ERR\n";
    }

    user* currentUser = user_handler::getInstance().getUser(loggedInUsername);
    if (currentUser == nullptr) {
        return "ERR\n";
    }

    mail* selectedMail = currentUser->getMail(mailId);
    if (selectedMail == nullptr || selectedMail->deleted) {
        return "ERR\n";
    }

    std::string mailContent = read_file((user_handler::getInstance().getSpoolDir() / "messages" / selectedMail->filename).string());
    return "OK\n" + mailContent;
}

std::string cmdDEL(std::vector<std::string>& received, const std::string& loggedInUsername) {
    if (loggedInUsername.empty() || received.size() < 2) {
    	printf("%s %zu\n", loggedInUsername.c_str(), received.size());
        return "ERR\n";
    }

    char* p;
    long mailId = strtol(received.at(1).c_str(), &p, 10);
    if (*p != 0) { 
        return "ERR\n";
    }

    user* currentUser = user_handler::getInstance().getUser(loggedInUsername);
    if (currentUser == nullptr) {
        return "ERR\n";
    }

    bool deleteResult = currentUser->delMail(mailId);
    return deleteResult ? "OK\n" : "ERR\n";
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
    // return std::ranges::equal(lhs, rhs, ichar_equals);
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), ichar_equals);
}