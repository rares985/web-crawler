CC=g++
CFLAGS=-g -Wall -O3 -funroll-loops -std=c++11

all: server client

build: server client

server: server.o
	$(CC) $(CFLAGS) server.o server.h -o server

client: client.o
	$(CC) $(CFLAGS) client.o client.h  -o client

server.o:
	$(CC) $(CFLAGS) -c server.cpp
client.o:
	$(CC) $(CFLAGS) -c client.cpp

clean: 
	rm server client $(wildcard *.stdout) $(wildcard *.stderr) $(wildcard *.o)