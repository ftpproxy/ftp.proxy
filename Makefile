
CC =		gcc
CFLAGS =	-O2 -Wall -ggdb

DIR =		ftpproxy-1.0.8
TAR =		ftpproxy-1.0.8

FTPPROXY =	main.o ftp.o ip-lib.o lib.o

TARGETS =	ftp.proxy


all:		$(TARGETS)
	-ctags *.[ch]

install:	all
	strip $(TARGETS)
	cp $(TARGETS) /usr/local/sbin
	cp *.1 /usr/local/man/man1


ftp.proxy:	$(FTPPROXY)
	$(CC) -o $@ $(FTPPROXY)


tar:		clean
	cd ..; tar cvf $(TAR).tar $(DIR); gzip $(TAR).tar
	mv ../$(TAR).tar.gz .
	
clean:
	rm -f *.o cut out $(TARGETS) $(TAR).tar.gz

