# All targets
all: patch search

patch: patch.o lab4_util.o lab4_start.o
	ld -m elf_x86_64 lab4_start.o patch.o lab4_util.o -o patch

search: search.o lab4_util.o lab4_start.o
	ld -m elf_x86_64 lab4_start.o search.o lab4_util.o -o search

patch.o: patch.c
	 gcc -m64 -Wall -ansi -c -nostdlib -fno-stack-protector patch.c -o patch.o

search.o: search.c
	 gcc -m64 -Wall -ansi -c -nostdlib -fno-stack-protector search.c -o search.o

lab4_util.o: lab4_util.c
	 gcc -m64 -Wall -ansi -c -nostdlib -fno-stack-protector lab4_util.c -o lab4_util.o

lab4_start.o: lab4_start.s
	nasm -f elf64 lab4_start.s -o lab4_start.o

#Clean the build directory
clean: 
	rm -f patch search *.o
