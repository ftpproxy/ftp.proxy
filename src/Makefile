
CC =		gcc
CFLAGS =	-O2 -Wall -ggdb -pedantic

FTPPROXY =	main.o ftp.o daemon.o ip-lib.o lib.o

TARGETS =	ftp.proxy


all:		$(TARGETS)
	-ctags *.[ch]

ftp.proxy:	$(FTPPROXY)
	$(CC) -o $@ $(FTPPROXY)

	
clean:
	rm -f *.o cut out $(TARGETS) $(TAR).tgz

