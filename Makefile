all: client server

client.o: client.c cdf.h
	gcc -c client.c cdf.h -pthread

client: client.o cdf.o conn.o
	gcc client.o cdf.o conn.o -o client -pthread -lm -lrt

server: server.c
	gcc server.c -o server -pthread

cdf.o: cdf.c cdf.h
	gcc -c cdf.c cdf.h

conn.o: conn.c conn.h
	gcc -c conn.c conn.h

clean:
	rm -f *.o
	rm -f client
	rm -f server


