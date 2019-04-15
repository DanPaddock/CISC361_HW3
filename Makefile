# choose your compiler
CC=gcc
#CC=gcc -Wall

mysh: sh.o get_path.o main.c linked_list.o 
	$(CC) -g main.c sh.o get_path.o linked_list.o -o mysh
#	$(CC) -g main.c sh.o get_path.o linked_list.o bash_getcwd.o -o mysh

sh.o: sh.c sh.h
	$(CC) -g -c sh.c

linked_list.o: linked_list.c linked_list.h
	$(CC) -g -c linked_list.c

get_path.o: get_path.c get_path.h
	$(CC) -g -c get_path.c

clean:
	rm -rf sh.o get_path.o mysh
