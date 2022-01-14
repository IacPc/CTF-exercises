all: server

server: server.c
	gcc -no-pie -g -O0 -fno-stack-protector -z now -o server server.c

.PHONY: clean
clean:
	rm -f server
