all: client server

client: client.c
	gcc -o client client.c 

server: server.c
	gcc server.c -o server -pthread

clean:
	rm -f *.o
	rm -f client
	rm -f server