
#
# Uncomment following for Solaris compile (need gnu make for top level
# make file)
#
#OSFLAG = -DSOLARIS
#OSLIB = -lnsl -lsocket

CC =		gcc
CFLAGS =	-O2 -Wall -ggdb -DVERSION=\"$(VERSION)\" ${OSFLAG}

FTPPROXY =	main.o config.o ftp.o daemon.o ip-lib.o lib.o

TARGETS =	ftp.proxy


all:		$(TARGETS)
	-ctags *.[ch]

ftp.proxy:	$(FTPPROXY)
	$(CC) -o $@ $(FTPPROXY) ${OSLIB}

	
clean:
	rm -f *.o cut out $(TARGETS) $(TAR).tgz

