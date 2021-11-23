#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define BUFF 1024

void usage() {
	printf("syntax : echo-client <ip> <port>\n");
	printf("sample : echo-client 192.168.10.2 1234\n");
}

// reads from selected file descriptor
int readFromFD(int fd, char buffer[])
{
	// storage to read into
	char tempBuff[101];
	// return value of read()
	int r;
	// # of characters in buffer[]
	int length;
	
	// can't store into buffer if buffer doesn't have enough space
	if (strlen(buffer)+101 >= BUFF)
		return 0;
	
	// if buffer isn't empty insert line-carry so message prints in different lines
	length = strlen(buffer);
	if (length) buffer[length] = '\n';
	
	while (1)
	{
		r = read(fd, tempBuff, 100);
		// error -> end program
		if(r == -1)
		{
			printf("%s", strerror(errno));
			return -1;
		}
		// didn't read any bytes -> stop reading
		else if (r == 0)
			break;
		// read just a line carry -> stop reading
		else if (r == 1 && tempBuff[0] == '\n')
			break;
		
		// to append if buffer isn't empty	
		length = strlen(buffer);
		
		strncpy(buffer+length, tempBuff, r);
		buffer[length+r] = '\0';
		
		// no more bytes to read -> break
		if (r < 100)
			break;
	}
	return 1;
}

// writes to selected file descriptor
int writeToFD(int fd, char buffer[])
{
	int temp;
	if (strlen(buffer) == 0)
		return 0;
	temp = write(fd, buffer, strlen(buffer));
	buffer[0] = '\0';
	
	return temp;
}

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		usage();
		return -1;
	}
	// sock fd of this client
	int sockfd;
	struct sockaddr_in connect_to;
	
	fd_set reads, writes, readsTEMP, writesTEMP;
	int result;
	
	// buffer to give to the Server, and to receive from the Server
	char messageToServer[BUFF] = {};
	char messageFromServer[BUFF] = {};
	
	if (argc != 3)
	{
		printf("Need two arguments: IP add., port #\n");
		return 0;
	}
	
	// open socket
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("%s", strerror(errno));
		perror("Socket Error");
		exit(1);
	}
	
	connect_to.sin_family = AF_INET;
	connect_to.sin_port = htons(atoi(argv[2]));
	connect_to.sin_addr.s_addr = inet_addr(argv[1]);
	
	// try to connect
	if (connect(sockfd, (struct sockaddr*)&connect_to, sizeof(struct sockaddr)) == -1)
	{
		printf("%s", strerror(errno));
		exit(1);
	}
	
	// reset fds to read and fds to write
	FD_ZERO(&reads);
	FD_ZERO(&writes);
	
	// set stdin and sockfd to reads
	FD_SET(0, &reads);
	FD_SET(sockfd, &reads);
	// set stdout and sockfd to writes
	FD_SET(1, &writes);
	FD_SET(sockfd, &writes);
	
	while(1)
	{
		// to reset every loop
		readsTEMP = reads;
		writesTEMP = writes;
		
		// select()
		result = select(sockfd+1, &readsTEMP, &writesTEMP, 0, 0);
		if (result == -1)
		{
			printf("select() error");
			exit(1);
		}
		// time is NULL so the program should never enter here (supposed to tell the user when no fds are avilable)
		else if (result == 0)
		{
			printf("select(): ");
		}
		// at least one of the sockets are available
		else
		{
			// is stdin available? (is their an input from the user?)
			if (FD_ISSET(0, &readsTEMP))
			{
				if (readFromFD(0, messageToServer) == -1)
				{
					printf("%s", strerror(errno));
					exit(-1);
				}
			}
			// is sockfd available? (did the Server send data)
			if (FD_ISSET(sockfd, &readsTEMP))
			{
				if (readFromFD(sockfd, messageFromServer) == -1)
				{
					printf("%s", strerror(errno));
					exit(-1);
				}
			}
			// is stdout available?
			if (FD_ISSET(1, &writesTEMP) && messageFromServer[0] != '\0')
			{
				if (write(1, "Message From Server: ", 21) == -1)
				{
					printf("%s", strerror(errno));
					exit(-1);
				}
				if (writeToFD(1, messageFromServer) == -1)
				{
					printf("%s", strerror(errno));
					exit(-1);
				}
			}
			// is sockfd abailable? (can the client send to the Server?)
			if (FD_ISSET(sockfd, &writesTEMP) && messageToServer[0] != '\0')
			{
				if (writeToFD(sockfd, messageToServer) == -1)
				{
					printf("%s", strerror(errno));
					exit(-1);
				}
			}
		}
	}
}
