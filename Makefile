
DIR =		ftpproxy-1.1.4
TAR =		ftpproxy-1.1.4


TARGETS =	ftp.proxy


all:	$(TARGETS)
	cd src; make all 

install:	all
	cd src; strip $(TARGETS)  &&  cp $(TARGETS) /usr/local/sbin
	cd doc; cp *.1 /usr/local/man/man1


ftp.proxy:
	cd src; make ftp.proxy 


tar:		clean
	cd ..; tar cvf $(TAR).tar $(DIR); gzip $(TAR).tar
	mv ../$(TAR).tar.gz .
	
clean:
	cd src; rm -f *.o cut out $(TARGETS)
	rm -f $(TAR).tar.gz

