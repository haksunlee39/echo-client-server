#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define BUFF 1024

int clientCount = 0;

// client struct
typedef struct client{
	int fd;
	struct sockaddr_in addr;
	int sin_size;
	char message[BUFF];
	char bmessage[BUFF];
}client;

// array struct for dynamic resizing
typedef struct {
	client *array;
	size_t used;
	size_t size;
} Array;

int checkInput(int argc, char* argv[])
{
	if(argc > 4) return -1;
	int i;
	
	int e = 0;
	int b = 0;
	
	for (i = 0; i < argc; i++)
	{
		if(argv[i][0] == '-')
		{
			if(strlen(argv[i]) != 2) return -1;
			
			if(argv[i][1] == 'e') e = 1;
			else if(argv[i][1] == 'b') b = 1;
			
			if (e+b == 0) return -1;
		}
	}
	
	if (b == 1 && e == 0) return -1;
	
	return e+b*10;
}

void usage() {
	printf("syntax : echo-server <port> [-e[-b]]\n");
	printf("sample : echo-server 1234 -e -b\n");
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
	//buffer[0] = '\0';
	
	return temp;
}

void clearBuffer(char buffer[])
{
	buffer[0] = '\0';
}

// initialize dynamic array
void initArray(Array *a, size_t initialSize) {
	a->array = malloc(initialSize * sizeof(client));
	a->used = 0;
	a->size = initialSize;
}

// insert data into dynamic array
void insertArray(Array *a, client element) {
	// is all allocated memory is used, reallocate with more memory
	if (a->used == a->size) {
		a->size += 5;
		a->array = realloc(a->array, a->size * sizeof(client));
	}
	a->array[a->used++] = element;
}

// free the dynamic array
void freeArray(Array *a) {
	free(a->array);
	a->array = NULL;
	a->used = a->size = 0;
}

int main(int argc, char* argv[])
{
	int inputCheckResult = checkInput(argc, argv);
	if (inputCheckResult == -1)
	{
		usage();
		return -1;
	}
	
	int i,j;

	// sock fd of this Server
	int sockfd;
	struct sockaddr_in my_addr;
	
	fd_set reads, writes, readsTEMP, writesTEMP;
	int result;
	
	// to store the highest sock Fd (to reduce time in select())
	int maxSockFD = -1;
	
	// dynamic array to store clients
	Array clients;
	// temp variable to copy into the dynamic array
	client tempClient;
	
	int echo = inputCheckResult % 10;
	int broadcast = inputCheckResult / 10;
	printf("%d  %d\n",echo, broadcast);
	
	// open socket
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("%s", strerror(errno));
		perror("Socket Error");
		exit(1);
	}
	
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
	{
		printf("%s", strerror(errno));
		perror("Setsockopt Error");
		exit(1);
	}
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(atoi(argv[1]));
	my_addr.sin_addr.s_addr = INADDR_ANY;
	
	// bind
	if (bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1)
	{
		printf("%s", strerror(errno));
		perror("Bind Error");
		exit(1);
	}
	
	// ready to connect to clients
	if (listen(sockfd, 10) == -1)
	{
		printf("%s", strerror(errno));
		perror("Listen Error");
		exit(1);
	}
	
	// initialize max sock FD
	maxSockFD = sockfd;
	// initialize clients dynamic array
	initArray(&clients, 1);
	// initialize select parameters
	FD_ZERO(&reads);
	FD_ZERO(&writes);
	
	FD_SET(1, &writes);
	FD_SET(sockfd, &reads);
	
	// initialize temp elemenet for clients
	tempClient.sin_size = sizeof(struct sockaddr_in);
	tempClient.message[0] = '\0';
	tempClient.bmessage[0] = '\0';
	
	while(1)
	{
		// to reset every loop
		readsTEMP = reads;
		writesTEMP = writes;
		
		//select()
		result = select(maxSockFD + 1, &readsTEMP, &writesTEMP, 0, 0);
		if (result == -1)
		{
			printf("select() error");
			exit(1);
		}
		// time is NULL so the program should never enter here (supposed to tell the user when no fds are available)
		else if (result == 0)
		{
			printf("select(): ");
		}
		// at least one of the sockets are available
		else
		{
			// is sockfd available? (is there a connect request?)
			if (FD_ISSET(sockfd, &readsTEMP))
			{
				tempClient.fd = accept(sockfd, (struct sockaddr*)&tempClient.addr, &tempClient.sin_size);
				
				if (maxSockFD < tempClient.fd)
					maxSockFD = tempClient.fd;
					
				FD_SET(tempClient.fd, &reads);
				FD_SET(tempClient.fd, &writes);
				
				insertArray(&clients, tempClient);
				
				clientCount++;
			}
			
			// for every client, check if they sent datas
			for (i = 0; i < clientCount; i++)
			{
				// if data is sent
				if (FD_ISSET(clients.array[i].fd, &readsTEMP))
				{
					if (readFromFD(clients.array[i].fd, clients.array[i].message) == -1 )
					{
						printf("%s", strerror(errno));
						exit(-1);
					}
				}
				if (FD_ISSET(1, &writesTEMP) && clients.array[i].message[0] != '\0')
				{
					/*if (write(1, "Message From Client: ", 21) == -1)
					{
						printf("%s", strerror(errno));
						exit(-1);
					}*/
					printf("Message From Client %d: \n\b\b", i);
					if (writeToFD(1, clients.array[i].message) == -1)
					{
						printf("%s", strerror(errno));
						exit(-1);
					}
				}
				for (j = 0; j < clientCount && broadcast; j++)
				{
					if (i == j) continue;
					
					strcpy(clients.array[j].bmessage, clients.array[i].message);
				}
				// if there is data to send and is writable
				if (FD_ISSET(clients.array[i].fd, &writesTEMP) && clients.array[i].message[0] != '\0')
				{
					if (echo)
					{
						if (writeToFD(clients.array[i].fd, clients.array[i].message) == -1)
						{
							printf("%s", strerror(errno));
							exit(-1);
						}
					}
					clearBuffer(clients.array[i].message);
				}
				if (FD_ISSET(clients.array[i].fd, &writesTEMP) && clients.array[i].bmessage[0] != '\0')
				{
					if (echo)
					{
						if (writeToFD(clients.array[i].fd, clients.array[i].bmessage) == -1)
						{
							printf("%s", strerror(errno));
							exit(-1);
						}
					}
					clearBuffer(clients.array[i].bmessage);
				}
			}
		}
	}
	
	
}
