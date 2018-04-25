CC = gcc
CFLAGS = -Wall -g
LIBFLAGS = $(CFLAGS) -fPIC -m64

all:
	make liblwp.so

liblwp.so: magic64.o lwp.o roundrobin.o
	$(CC) $(LIBFLAGS) -shared -o $@ $^

magic64.o: magic64.S
	$(CC) -o $@ -c $<

lwp.o: lwp.c
	$(CC) $(LIBFLAGS) -o $@ -c $<

roundrobin.o: roundrobin.c
	$(CC) $(LIBFLAGS) -o $@ -c $<

clean:
	rm -f *.o