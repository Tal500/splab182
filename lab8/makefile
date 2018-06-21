# All Targets
all: myELF

# Tool invocations
myELF: main.o
	gcc -o myELF main.o

# Depends on the source and header files
main.o: main.c
	gcc -g -Wall -c -Iinclude -o main.o main.c

test: myELF
	valgrind --leak-check=full --show-reachable=yes ./myELF myELF

#Clean the build directory
clean: 
	rm -f myELF *.o
