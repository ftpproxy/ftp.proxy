# ftp.proxy
ftp.proxy is an application level gateway for FTP. It sits between a client and a server forwarding command and data streams supporting a subset of the file transfer protocol as described in RFC 959.

Beside this basic function which makes the program useful on firewall or masqueraders it offers fixing the FTP server (e.g. for connections into a protected LAN) and proxy authentication.

ftp.proxy offers external access control programs (ACP), that is an external program decides if a user can use the proxy service or not. This decision can be made on username, host name, day of time etc.
Since version 1.0.1 ftp.proxy supports also command control programs. Similar to ACPs the CCPs permit or deny access to certain FTP commands on the server, e.g. it's possible to allow a user to get files but to deny any kind of uploads.

## 1. Building

	Same old story:
	
	make
	make install
	
	If you like to compile ftp.proxy on a SystemV OS (like Solaris) please uncomment the following 
  two lines in src/Makefile:
	
	OSFLAG = -DSOLARIS
	OSLIB = -lnsl -lsocket
	
	For BSD like systems you must use gmake instead of make!

	On MacOS X is could be necessary to change the compiler setting in src/Makefile to: 
	
	CC =            cc

	Note: You must have gnu make and gcc installed on your system.



## 2. Installation

	After you sucessfully built and installed the daemon, you add an
	entry to your system's inetd.conf depending on your needs:

	-  clientside server selection: 

		ftp     stream  tcp     nowait  nobody  /usr/sbin/tcpd  /usr/local/sbin/ftp.proxy -e

	- clientside server selection with enhanced logging: 

		ftp     stream  tcp     nowait  nobody  /usr/sbin/tcpd  /usr/local/sbin/ftp.proxy -e -l -m

	- only to one particular FTP-Server:

		ftp     stream  tcp     nowait  nobody  /usr/sbin/tcpd  /usr/local/sbin/ftp.proxy my.outside.server 
	
	For xinetd this must work (example):

		service ftp
			{
			 socket_type = stream
			 wait        = no
			 user        = nobody 
			 server      = /usr/local/sbin/ftp.proxy 
			 server_args = -e -m
			}
		

	If you like to seprate the ftp.proxy log messages from the other stuff,
	try something like this in your syslog.conf:

	*.*;ftp.none                            -/var/log/messages
	ftp.*                                   /var/log/ftp


	

## 3. Advanced Features

	ftp.proxy has some advanced features not found in other FTP proxies:
	advanced access control, command control and monitor mode.  They are
	explained in the manpage.
