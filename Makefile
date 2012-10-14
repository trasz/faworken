all: wtest fwkhub

wtest: wtest.c window.c map.c remote.c
	$(CC) -o wtest wtest.c window.c map.c remote.c -lcurses -ggdb -Wall

fwkhub: fwkhub.c map.c
	$(CC) -o fwkhub fwkhub.c map.c remote.c -ggdb -Wall

clean:
	rm -rf wtest fwkhub *.o *.core *.dSYM reports
