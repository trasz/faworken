all: fwk fwkhub

fwk: fwk.c window.c map.c remote.c
	$(CC) -o fwk fwk.c window.c map.c remote.c -lcurses -ggdb -Wall

fwkhub: fwkhub.c map.c remote.c
	$(CC) -o fwkhub fwkhub.c map.c remote.c -ggdb -Wall

clean:
	rm -rf fwk fwkhub *.o *.core *.dSYM reports
