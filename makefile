all: client-server

client-server: server.o client.o
	g++ -o server server.o
	g++ -o client client.o

server.o: server.c
client.o: client.c

clean:
	rm -f server
	rm -f client
	rm -f *.o
