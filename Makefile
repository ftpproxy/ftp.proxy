
VERSION =	1.2.2
DIR =		ftpproxy-$(VERSION)
TAR =		ftpproxy-$(VERSION)

export VERSION

TARGETS =	ftp.proxy


all:	$(TARGETS)
	cd src; make all 


install:	all
	cd src; strip $(TARGETS)  &&  cp $(TARGETS) /usr/local/sbin
	cd doc; cp *.1 /usr/local/man/man1


ftp.proxy:
	cd src; make ftp.proxy 


tar:		clean
	cd ..; tar cvzf $(TAR).tgz $(DIR)
	mv ../$(TAR).tgz .
	
clean:
	cd src; rm -f *.o cut out $(TARGETS)
	rm -f $(TAR).tgz
	rm -f src/tags
