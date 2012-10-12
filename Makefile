all: wtest

wtest: wtest.c win.c map.c
	$(CC) -o wtest *.c -lcurses -ggdb -Wall

clean:
	rm -f wtest *.o *.core
