all: wtest

wtest: wtest.c window.c map.c
	$(CC) -o wtest *.c -lcurses -ggdb -Wall

clean:
	rm -f wtest *.o *.core
