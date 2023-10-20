#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
	int create_socket;
	char buffer[BUF];
	struct sockaddr_in address;
	int size;
	int isQuit;

	////////////////////////////////////////////////////////////////////////////
	// CREATE A SOCKET
	// https://man7.org/linux/man-pages/man2/socket.2.html
	// https://man7.org/linux/man-pages/man7/ip.7.html
	// https://man7.org/linux/man-pages/man7/tcp.7.html
	// IPv4, TCP (connection oriented), IP (same as server)
	if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Socket error");
		return EXIT_FAILURE;
	}

	////////////////////////////////////////////////////////////////////////////
	// INIT ADDRESS
	// Attention: network byte order => big endian
	memset(&address, 0, sizeof(address)); // init storage with 0
	address.sin_family = AF_INET;         // IPv4
	// https://man7.org/linux/man-pages/man3/htons.3.html
	address.sin_port = htons(PORT);
	// https://man7.org/linux/man-pages/man3/inet_aton.3.html
	if (argc < 2)
	{
		inet_aton("127.0.0.1", &address.sin_addr);
	}
	else
	{
		inet_aton(argv[1], &address.sin_addr);
	}

	////////////////////////////////////////////////////////////////////////////
	// CREATE A CONNECTION
	// https://man7.org/linux/man-pages/man2/connect.2.html
	if (connect(create_socket,
				(struct sockaddr *)&address,
				sizeof(address)) == -1)
	{
		// https://man7.org/linux/man-pages/man3/perror.3.html
		perror("Connect error - no server available");
		return EXIT_FAILURE;
	}

	// ignore return value of printf
	printf("Connection with server (%s) established\n",
			 inet_ntoa(address.sin_addr));

	////////////////////////////////////////////////////////////////////////////
	// RECEIVE DATA
	// https://man7.org/linux/man-pages/man2/recv.2.html
	/*
	size = recv(create_socket, buffer, BUF - 1, 0);
	if (size == -1)
	{
		perror("recv error");
	}
	else if (size == 0)
	{
		printf("Server closed remote socket\n"); // ignore error
	}
	else
	{
		buffer[size] = '\0';
		printf("%s", buffer); // ignore error
	}
	*/

	do {
		printf("Please specify a command (SEND, LIST, READ, DEL, QUIT): ");
		if (fgets(buffer, BUF - 1, stdin) != NULL)
		{
			size = strlen(buffer);
			if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
			{
				size -= 2;
					buffer[size] = 0;
				}
				else if (buffer[size - 1] == '\n')
				{
					--size;
					buffer[size] = 0;
				}

				isQuit = strcmp(buffer, "QUIT") == 0;

				if (strcmp(buffer, "SEND") == 0)
				{
					char sender[BUF], receiver[BUF], subject[81], message[BUF * 10];
					printf("Sender: ");
					fgets(sender, BUF - 1, stdin);
					printf("Receiver: ");
					fgets(receiver, BUF - 1, stdin);
					printf("Subject: ");
					fgets(subject, 80, stdin);
					printf("Message: ");
					char line[BUF];
					message[0] = '\0';
					while (true)
					{
						fgets(line, BUF - 1, stdin);
						if (strcmp(line, ".\n") == 0)
							break;
						strcat(message, line);
					}
					snprintf(buffer, sizeof(buffer), "SEND\n%s%s%s%s.\n", sender, receiver, subject, message);
				}
				else if (strcmp(buffer, "LIST") == 0)
				{
					char username[BUF];
					printf("Username: ");
					fgets(username, BUF - 1, stdin);
					snprintf(buffer, sizeof(buffer), "LIST\n%s", username);
				}
				else if (strcmp(buffer, "READ") == 0)
				{
					char username[BUF], msgNum[10];
					printf("Username: ");
					fgets(username, BUF - 1, stdin);
					printf("Message Number: ");
					fgets(msgNum, 9, stdin);
					snprintf(buffer, sizeof(buffer), "READ\n%s%s", username, msgNum);
				}
				else if (strcmp(buffer, "DEL") == 0)
				{
					char username[BUF], msgNum[10];
					printf("Username: ");
					fgets(username, BUF - 1, stdin);
					printf("Message Number: ");
					fgets(msgNum, 9, stdin);
					snprintf(buffer, sizeof(buffer), "DEL\n%s%s", username, msgNum);
				}

				//////////////////////////////////////////////////////////////////////
				// SEND DATA
				// https://man7.org/linux/man-pages/man2/send.2.html
				// send will fail if connection is closed, but does not set
				// the error of send, but still the count of bytes sent
				int size = strlen(buffer);

				if ((send(create_socket, buffer, size + 1, 0)) == -1) 
				{
					// in case the server is gone offline we will still not enter
					// this part of code: see docs: https://linux.die.net/man/3/send
					// >> Successful completion of a call to send() does not guarantee 
					// >> delivery of the message. A return value of -1 indicates only 
					// >> locally-detected errors.
					// ... but
					// to check the connection before send is sense-less because
					// after checking the communication can fail (so we would need
					// to have 1 atomic operation to check...)
					perror("send error");
					break;
				}

			//////////////////////////////////////////////////////////////////////
			// RECEIVE FEEDBACK
			// consider: reconnect handling might be appropriate in somes cases
			//           How can we determine that the command sent was received 
			//           or not? 
			//           - Resend, might change state too often. 
			//           - Else a command might have been lost.
			//
			// solution 1: adding meta-data (unique command id) and check on the
			//             server if already processed.
			// solution 2: add an infrastructure component for messaging (broker)
			//
			size = recv(create_socket, buffer, BUF - 1, 0);
			if (size == -1)
			{
				perror("recv error");
				break;
			}
			else if (size == 0)
			{
				printf("Server closed remote socket\n"); // ignore error
				break;
			}
			else
			{
				buffer[size] = '\0';
				printf("<< %s\n", buffer); // ignore error
				/*if (strcmp("OK", buffer) != 0) // needs proper verification, since responses vary between commands
				{
					fprintf(stderr, "<< Server error occured, abort\n");
					break;
				}*/
			}
		}
	} while (!isQuit);

	////////////////////////////////////////////////////////////////////////////
	// CLOSES THE DESCRIPTOR
	if (create_socket != -1)
	{
		if (shutdown(create_socket, SHUT_RDWR) == -1)
		{
			// invalid in case the server is gone already
			perror("shutdown create_socket"); 
		}
		if (close(create_socket) == -1)
		{
			perror("close create_socket");
		}
		create_socket = -1;
	}

	return EXIT_SUCCESS;
}
