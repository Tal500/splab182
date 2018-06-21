# All Targets
all: hexeditplus

# Tool invocations
hexeditplus: main.o
	gcc -o hexeditplus main.o

# Depends on the source and header files
main.o: main.c
	gcc -g -Wall -c -Iinclude -o main.o main.c

test: hexeditplus
	valgrind --leak-check=full --show-reachable=yes ./hexeditplus

#Clean the build directory
clean: 
	rm -f hexeditplus *.o
