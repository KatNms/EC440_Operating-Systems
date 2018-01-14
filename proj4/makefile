CC=gcc -Wall -g -m32
all: threadlib main
	$(CC) -o main main.o tls.o -lpthread -Wall

main: main.c
	$(CC) -c -o main.o main.c

threadlib: tls.c
	$(CC) -c -o tls.o tls.c

clean:
	rm tls.o main.o main
