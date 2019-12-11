all:
	gcc -pthread server.c -o server
	gcc client.c -o client
