CC=gcc
CFLAGS=-I.

all: 
	$(CC) -m64 -c test.c -o test.o
	$(CC) -m64 -c p_mmap.c -o p_mmap.o
	$(CC) -m64 -static -o ptest test.o p_mmap.o -I.
clean:
	@rm -rf *.o ptest


