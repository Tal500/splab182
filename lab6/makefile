# All targets
all: myshell

myshell: myshell.o line_parser.o job_control.o
	gcc myshell.o line_parser.o job_control.o -o myshell

myshell.o: line_parser.h job_control.h myshell.c
	 gcc -g -Wall -c -Iinclude -o myshell.o myshell.c

line_parser.o: line_parser.h line_parser.c
	 gcc -g -Wall -c -Iinclude line_parser.c -o line_parser.o

job_control.o: job_control.h job_control.c
	 gcc -g -Wall -c -Iinclude job_control.c -o job_control.o

#Clean the build directory
clean: 
	rm -f myshell *.o
