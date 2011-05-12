
#
# Uncomment following for Solaris compile (need gnu make for top level
# make file)
#
#OSFLAG = -DSOLARIS
#OSLIB = -lnsl -lsocket


#
# Compile time options
#
#   FTP_FILECOPY	Compile code for copies of transfered files.
#   FACILITY_NAMES	Compile code for syslog facilities by name,
#   			requires compiler OS support.
#

OPTIONS =	-DFTP_FILECOPY -DFACILITY_NAMES


CC =		gcc
CFLAGS =	-O2 -Wall -ggdb -DVERSION=\"$(VERSION)\" ${OSFLAG} $(OPTIONS)

FTPPROXY =	main.o config.o ftp.o daemon.o procinfo.o filecopy.o ip-lib.o lib.o

TARGETS =	ftp.proxy


all:		$(TARGETS)
	-ctags *.[ch]

ftp.proxy:	$(FTPPROXY)
	$(CC) -o $@ $(FTPPROXY) ${OSLIB}
	strip ftp.proxy

clean:
	rm -f *.o cut out tags $(TARGETS) $(TAR).tgz

