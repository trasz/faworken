all: wtest

wtest: wtest.c wieland.c
	$(CC) -o wtest *.c -lcurses -ggdb -Wall

clean:
	rm -f wtest *.o *.core
