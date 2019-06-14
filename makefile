my_server: my_server.o my_recv.o
	gcc my_server.o my_recv.o -o my_server
my_client: my_client.o my_recv.o
	gcc my_client.o my_recv.o -o my_client
my_server.o: my_server.c
	gcc -c my_server.c
my_client.o: my_client.c
	gcc -c my_client.c
my_recv.o: my_recv.c
	gcc -c my_recv.c
clean:
	rm -f *.o
