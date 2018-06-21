# All Targets
all: client server

# Tool invocations
client: common.o client.o
	gcc -o client common.o client.o

server: common.o server.o
	gcc -o server common.o server.o

# Depends on the source and header files
common.o: common.c common.h
	gcc -g -Wall -c -Iinclude -o common.o common.c

client.o: client.c common.h
	gcc -g -Wall -c -Iinclude -o client.o client.c

server.o: server.c common.h
	gcc -g -Wall -c -Iinclude -o server.o server.c

test: client
	valgrind --leak-check=full --show-reachable=yes ./client

#Clean the build directory
clean: 
	rm -f client server *.o
