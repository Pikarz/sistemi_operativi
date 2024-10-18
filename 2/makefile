.PHONY: clean
all: 2.server 2.client
	
2.server: 2.server.c
	gcc 2.server.c -o 2.server

2.client: 2.client.c
	gcc 2.client.c -o 2.client

clean: 
	rm -f 2.server 2.client
