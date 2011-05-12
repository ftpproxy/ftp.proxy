
PROGRAM =	ftpproxy
VERSION =	2.1.0-beta5
DIR =		$(PROGRAM)-$(VERSION)
TAR =		$(PROGRAM)-$(VERSION)

DEBRELEASE =    1
TAR =           $(PROGRAM)-$(VERSION)
DIR =           $(PROGRAM)-$(VERSION)

INSTALL_PREFIX =


export VERSION

TARGETS =	ftp.proxy


all:	$(TARGETS)
	cd src; make all 


install:	all
	mkdir -p $(INSTALL_PREFIX)/usr/local/sbin
	mkdir -p $(INSTALL_PREFIX)/usr/local/man/man1
	cd src; strip $(TARGETS)  &&  cp $(TARGETS) $(INSTALL_PREFIX)/usr/local/sbin
	cd doc; cp *.1 $(INSTALL_PREFIX)/usr/local/man/man1


ftp.proxy:
	cd src; make ftp.proxy 

debian:		ftp.proxy
	deb-packager $(PROGRAM) $(VERSION)+$(DEBRELEASE) debian/files.list


tar:		clean
	cd ..; tar cvzf $(TAR).tgz $(DIR)
	mv ../$(TAR).tgz .
	
clean:
	cd src; rm -f *.o cut out $(TARGETS)
	rm -f $(TAR).tgz *.deb
	rm -f src/tags

