# All targets
all: myshell

myshell: main.o line_parser.o job_control.o
	gcc main.o line_parser.o job_control.o -o myshell

main.o: line_parser.h job_control.h main.c
	 gcc -g -Wall -c -Iinclude -o main.o main.c

line_parser.o: line_parser.h line_parser.c
	 gcc -g -Wall -c -Iinclude line_parser.c -o line_parser.o

job_control.o: job_control.h job_control.c
	 gcc -g -Wall -c -Iinclude job_control.c -o job_control.o

LEAKCHECK := valgrind --leak-check=full -v --show-reachable=yes

#Clean the build directory
clean: 
	rm -f myshell *.o
