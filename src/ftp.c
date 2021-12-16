
/*

    File: ftpproxy/ftp.c

    Copyright (C) 1999, 2000  Wolfgang Zekoll  <wzk@quietsche-entchen.de>
    Copyright (C) 2000 - 2009  Andreas Schoenberg  <asg@ftpproxy.org>
  
    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
  
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
  
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/time.h>

#include "ip-lib.h"
#include "ftp.h"
#include "procinfo.h"
#include "lib.h"

#if defined (__linux__)
#  include <limits.h>
#  include <linux/netfilter_ipv4.h>
#endif

typedef struct _ftpcmd {
    char name[20];
    int par;    // parameter
    int ispath; //
    int useccp; //
    int resp;
    int log;
    } ftpcmd_t;

ftpcmd_t cmdtab[] = {

	/*
	 * Einfache FTP Kommandos.
	 */

    { "ABOR", 0, 0, 0,	225, 1 },		/* oder 226 */
    { "ACCT", 1, 0, 0,	230, 0 },
    { "CDUP", 1, 1, 1,	200, 1 },
    { "CWD",  1, 1, 1,	250, 1 },
    { "DELE", 1, 1, 1,	250, 1 },
    { "NOOP", 0, 0, 0,	200, 0 },
    { "MDTM", 1, 1, 1,	257, 1 },
    { "MKD",  1, 1, 1,	257, 1 },
    { "MODE", 1, 0, 0,	200, 0 },
    { "PWD",  0, 0, 0,	257, 0 },
    { "QUIT", 0, 0, 0,	221, 0 },
    { "REIN", 0, 0, 0,	0, /* 220, */ 0 },	/* wird nicht unterstuetzt */
    { "REST", 1, 0, 0,	350, 0 },
    { "RNFR", 1, 1, 1,	350, 1 },
    { "RNTO", 1, 1, 1,	250, 1 },
    { "RMD",  1, 1, 1,	250, 1 },
    { "SITE", 1, 0, 1,	200, 0 },
    { "SIZE", 1, 1, 1,	213, 1 },
    { "SMNT", 1, 0, 0,	250, 0 },
    { "STAT", 1, 1, 1,	211, 0 },			/* oder 212, 213 */
    { "STRU", 1, 0, 0,	0, /* 200, */ 0 },	/* wird nicht unterstuetzt */
    { "SYST", 0, 0, 0,	215, 0 },
    { "TYPE", 1, 0, 0,	200, 0 },
    { "XCUP", 1, 1, 1,	200, 1 },
    { "XCWD", 1, 1, 1,	250, 1 },
    { "XMKD", 1, 1, 1,	257, 1 },
    { "XPWD", 0, 0, 0,	257, 0 },
    { "XRMD", 1, 1, 1,	250, 1 },

	/*
	 * Nur der Vollstaendigkeit halber: FTP Kommandos die gesondert
	 * behandelt werden.
	 */

    { "LIST", 1, 1, 1,	0, 0 },
    { "NLST", 1, 1, 1,	0, 0 },
    { "PORT", 1, 0, 0,	0, /* 200, */ 0 },
    { "EPRT", 1, 0, 0,	0, /* 200, */ 1 }, /* RFC2428 */
    { "PASV", 0, 0, 0,	0, /* 200, */ 0 },
    { "EPSV", 0, 0, 0,	0, /* 200, */ 1 }, /* RFC2428 */
    { "ALLO", 1, 0, 0,	0, /* 200, */ 0 },
    { "RETR", 1, 1, 1,	0, 0 },
    { "STOR", 1, 1, 1,	0, 0 },
    { "STOU", 0, 0, 1,	0, 0 },
    { "APPE", 1, 1, 1,	0, 0 },
    { "HELP", 0, 0, 0,	0, 0 },
    { "FEAT", 0, 0, 1,	0, 0 },
    { "",     0, 0, 0,	0, 0 }
    };




int get_client_info(ftp_t *x, int pfd)
{
	unsigned int size;
	struct sockaddr * saddr = w_sockaddr_new (use_ipv6);
	int ret;
	*x->client.name = 0;
	size = w_sockaddr_get_size (saddr);
	if (getpeername(pfd, saddr, &size) < 0 ) {
		free (saddr);
		return (-1);
	}
	w_sockaddr_get_ip_str (saddr, x->client.ipnum, sizeof(x->client.ipnum));

	if (x->config->numeric_only == 1)
		copy_string(x->client.name, x->client.ipnum, sizeof(x->client.name));
	else {
		ret = getnameinfo (saddr, size, x->client.name, sizeof(x->client.name), NULL, 0, 0);
		if (ret != 0) {
			*(x->client.name) = 0; // error, make sure string is empty
		}
	}

	strlwr(x->client.name);

	free (saddr);
	return (0);
}


	/*
	 * Basic I/O functions
	 */

int close_ch(ftp_t *x, dtc_t *ch)
{
	if (ch->isock >= 0)
		close(ch->isock);

	if (ch->osock >= 0)
		close (ch->osock);

	ch->isock     = -1;
	ch->osock     = -1;
	ch->state     = 0;
	ch->operation = 0;
	ch->seen150   = 0;

#ifdef FTP_FILECOPY
	if (ch->copyfd >= 0) {
		close (ch->copyfd);
		ch->copyfd = -1;
		x->cp.create = 0;
		}
#endif

	return (0);
}

int getc_fd(ftp_t *x, int fd)
{
	int	c;
	bio_t	*bio;

	if (fd == 0)
		bio = &x->cbuf;
	else if (fd == x->fd.server)
		bio = &x->sbuf;
	else {
		printerror(1 | ERR_SYSTEM, "-ERR", "internal bio/fd error");
		exit (1);
		}

	if (bio->here >= bio->len) {
		int	rc, max, bytes, earlyreported;
		struct timeval tov;
		fd_set	available, fdset;

		bio->len = bio->here = 0;
		earlyreported = 0;

		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);
/*		x->fd.max = fd; */
		max = fd;

		if (x->ch.operation == 0)
			/* nichts */ ;
		else if (x->ch.state == PORT_LISTEN) {
			if (x->ch.mode == MODE_PORT) {
				FD_SET(x->ch.osock, &fdset);
				if (x->ch.osock > max)
					max = x->ch.osock;

				x->ch.active = x->ch.osock;
				}
			else if (x->ch.mode == MODE_PASSIVE) {
				FD_SET(x->ch.isock, &fdset);
				if (x->ch.isock > max)
					max = x->ch.isock;

				x->ch.active = x->ch.isock;
				}
			else {
				printerror(1 | ERR_SYSTEM, "-ERR", "internal mode error");
				exit (1);
				}
			}
		else if (x->ch.state == PORT_CONNECTED  &&  x->ch.seen150 == 1) {
			FD_SET(x->ch.active, &fdset);
			if (x->ch.active > max)
				max = x->ch.active;
			}
			
		bytes = 0;
		while (1) {
			available = fdset;
			tov.tv_sec  = x->config->timeout;
			tov.tv_usec = 0;

			if (debug >= 2)
				fprintf (stderr, "select max= %d\n", max);

			rc = select(max + 1, &available, (fd_set *) NULL, (fd_set *) NULL, &tov);
			if (rc < 0) {
				printerror(0, "", "select() error: %s\n", strerror(errno));
				break;
				}
			else if (rc == 0) {
				printerror(1 | ERR_TIMEOUT, "-ERR", "connection timed out: client= %s, server= %s:%u",
					x->client.name, x->server.name, x->server.port);
/*
 *				printerror(0, "", "connection timed out: client= %s, server= %s:%u",
 *					x->client.name, x->server.name, x->server.port);
 *				return (-1);
 */
				}

			if (FD_ISSET(fd, &available)) {
				if ((bytes = read(fd, bio->buffer, sizeof(bio->buffer) - 2)) <= 0) {
					if (debug != 0) {
						if (bytes == 0)
							fprintf (stderr, "received zero bytes on fd %d\n", fd);
						else
							fprintf (stderr, "received %d bytes on fd %d, errno= %d, error= %s\n", bytes, fd, errno, strerror(errno));
						}

					return (-1);
					}

				break;
				}
			else if (FD_ISSET(x->ch.active, &available)) {
				if (x->ch.state == PORT_LISTEN) {
					unsigned int adrlen;
					int	sock;
					struct sockaddr * adr = w_sockaddr_new (use_ipv6);

					earlyreported = 0;
					adrlen = w_sockaddr_get_size (adr);
					sock = accept(x->ch.active, adr, &adrlen);
					if (debug != 0)
						fprintf (stderr, "accept() on socket\n");

					if (sock < 0) {
						printerror(1 | ERR_SYSTEM, "-ERR", "accept error: %s", strerror(errno));
						free (adr);
						exit (1);
						}
					else {
						char	remote[80];
						w_sockaddr_get_ip_str (adr, remote, sizeof(remote));
						free (adr);

						if (debug != 0)
							fprintf (stderr, "connection from %s\n", remote);

						/*
						 * Gegenstelle ueberpruefen.
						 */

						if (x->ch.mode == MODE_PORT) {
							if (strcmp(x->server.ipnum, remote) != 0) {
								if (x->config->allow_anyremote != 0)
									/* configuration tells us not to care -- 31JAN02asg */ ;
								else {
									printerror(1 | ERR_OTHER, "-ERR", "unexpected connect: %s, expected= %s", remote, x->server.ipnum);
									exit (1);
									}
								}
							}
						else {
							if (strcmp(x->client.ipnum, remote) != 0) {
								if (x->config->allow_anyremote != 0)
									/* ok -- 31JAN02asg */ ;
								else {
									printerror(1 | ERR_OTHER, "-ERR", "unexpected connect: %s, expected= %s", remote, x->client.ipnum);
									exit (1);
									}
								}
							}
						}

					/*
					 * Datenkanal zur anderen Seite aufbauen.
					 */

					if (x->ch.mode == MODE_PORT) {
						dup2(sock, x->ch.osock);
						close (sock);
						x->ch.state = PORT_CONNECTED;
						if (debug != 0)
							fprintf (stderr, "osock= %d\n", x->ch.osock);

						if ((x->ch.isock = openip(x->ch.client.ipnum, x->ch.client.port, x->i.ipnum, x->config->dataport)) < 0) {
							printerror(1 | ERR_CLIENT, "-ERR", "can't connect to client: %s", strerror(errno));
							exit (1);
							}

						if (debug != 0)
							fprintf (stderr, "isock= %d\n", x->ch.isock);
						}
					else if (x->ch.mode == MODE_PASSIVE) {
						dup2(sock, x->ch.isock);
						close (sock);
						x->ch.state = PORT_CONNECTED;
						if (debug != 0)
							fprintf (stderr, "isock= %d\n", x->ch.isock);

						if ((x->ch.osock = openip(x->ch.server.ipnum, x->ch.server.port, x->config->sourceip, 0)) < 0) {
							printerror(ERR_SERVER | 1, "-ERR", "can't connect to server: %s", strerror(errno));
							exit (1);
							}

						if (debug != 0)
							fprintf (stderr, "osock= %d\n", x->ch.osock);
						}


					/*
					 * Setzen der Datenquelle (Server oder Client).
					 */

					if (x->ch.operation == OP_GET) {
						x->ch.active = x->ch.osock;
						x->ch.other  = x->ch.isock;
						}
					else if (x->ch.operation == OP_PUT) {
						x->ch.active = x->ch.isock;
						x->ch.other  = x->ch.osock;
						}
					else {
						printerror(ERR_SYSTEM | 1, "-ERR", "transfer operation error");
						exit (1);
						}

					if (x->ch.seen150 == 0) {

						/*
						 * And finally ... another attempt to solve the short
						 * data transmission timing problem: If we didn't receive
						 * the 150 response yet from the server we deactivate the
						 * data channel until we have the 150 -- 030406asg
						 */

						if (debug >= 2)
							fprintf (stderr, "150 not seen, deactivating data channel\n");

						FD_ZERO(&fdset);
						FD_SET(fd, &fdset);
						max = fd;
						}
					else {
						if (debug >= 2)
							fprintf (stderr, "150 already seen, activating data channel\n");

						FD_ZERO(&fdset);
						FD_SET(fd, &fdset);
						FD_SET(x->ch.active, &fdset);
						max = (fd > x->ch.active)? fd: x->ch.active;
						}

					if (debug != 0)
						fprintf (stderr, "active= %d, other= %d\n", x->ch.active, x->ch.other);

					x->ch.bytes = 0;
					gettimeofday(&x->ch.start2, NULL);
					}
				else if (x->ch.state == PORT_CONNECTED) {
					int	wrote;
					char	buffer[FTPMAXBSIZE + 10];

					if (x->ch.operation == 0) {
						if (earlyreported == 0) {
							earlyreported = 1;
							printerror(0, "", "early write/read event, sleeping 2 seconds");
							sleep(2);
							continue;
							}
						}

					bytes = read(x->ch.active, buffer, x->config->bsize);
					if (x->ch.operation == OP_GET)
						x->btc += bytes;
					else
						x->bts += bytes;

#ifdef FTP_FILECOPY
					if (x->ch.copyfd >= 0)
						write(x->ch.copyfd, buffer, bytes);

#endif
					/*
					 * Handling servers that close the data connection -- 24APR02asg
					 */

					wrote = 0;
					if ((bytes > 0)  &&  ((wrote = write(x->ch.other, buffer, bytes)) == bytes))
						x->ch.bytes += bytes;
					else {
						if (wrote < 0)
							printerror(0, "", "error writing data channel, error= %s", strerror(errno));

						if (debug != 0)
							fprintf (stderr, "closing data connection\n");

#ifdef FTP_FILECOPY
						writeinfofile(x, "001 Transfer complete");
#endif

						close_ch(x, &x->ch);
						FD_ZERO(&fdset);
						FD_SET(fd, &fdset);
						max = fd;

						return (1);
						}
					}
				}
			}

		bio->len  = bytes;
		bio->here = 0;
		}

	if (bio->here >= bio->len)
		return (-1);

	c = (unsigned char) bio->buffer[bio->here++];
	return (c);
}

char *readline_fd(ftp_t *x, int fd, char *line, int size)
{
	int	c, k;

	*line = 0;
	size = size - 2;

	c = getc_fd(x, fd);
	if (c < 0)
		return (NULL);
	else if (c == 1) {
		strcpy(line, "\001");
		return (line);
		}

	k = 0;
	while (c > 0  &&  c != '\n'  &&  c != 0) {
		if (k < size)
			line[k++] = c;

		c = getc_fd(x, fd);
		}

	line[k] = 0;
	noctrl(line);

	k = 0;
	while ((c = (unsigned char ) line[k]) != 0  &&  c > 126)
		k++;

	if (k > 0)
		copy_string(line, &line[k], size);

	return (line);
}


char *cfgets(ftp_t *x, char *line, int size)
{
	char	*p;

	*line = 0;
	if ((p = readline_fd(x, 0, line, size)) == NULL)
		return (NULL);
	else if (debug != 0)
		fprintf (stderr, "CLI >>>: %s\n", p);

	return (line);
}

int cfputs(ftp_t *x, char *line)
{
	char	buffer[310];

	if (debug != 0)
		fprintf (stderr, ">>> CLI: %s\n", line);

	snprintf (buffer, sizeof(buffer) - 2, "%s\r\n", line);
	write(1, buffer, strlen(buffer));

	return (0);
}


char *sfgets(ftp_t *x, char *line, int size)
{
	char *p;

	*line = 0;
	if ((p = readline_fd(x, x->fd.server, line, size)) == NULL)
		return (NULL);
	else if (debug != 0)
		fprintf (stderr, "SVR >>>: %s\n", p);

	return (line);
}

int sfputs(ftp_t *x, char *format, ...)
{
	int	len;
	char	buffer[310];
	va_list	ap;

	va_start(ap, format);
	vsnprintf (buffer, sizeof(buffer) - 10, format, ap);
	va_end(ap);

	if (debug != 0)
		fprintf (stderr, ">>> SVR: %s\n", buffer);

	/*
	 * There are firewalls that don't like command to be split in
	 * two packets.  Notice: the `- 10' above is really important
	 * to protect the proxy against buffer overflows.
	 */

	strcat(buffer, "\r\n");
	len = strlen(buffer);

	/*
	 * SIGPIPE is catched but then ignored, we have to handle it
	 * one our own now -- 24APR02asg
	 */

	if (write(x->fd.server, buffer, len) != len) {
		printerror(ERR_SERVER | 1, "-ERR", "error writing control connect, error= %s", strerror(errno));
		exit (1);
		}

	return (0);
}

int sfputc(ftp_t *x, char *command, char *parameter, char *line, int size, char **here)
{
	int	rc;
	char	*p, buffer[600];
	static char lastcmd[600] = "";

	if (command != NULL  &&  *command != 0) {
		if (parameter != NULL  &&  *parameter != 0)
			snprintf (buffer, sizeof(buffer) - 2, "%s %s", command, skip_ws(parameter));
		else
			copy_string(buffer, command, sizeof(buffer));

		sfputs(x, "%s", buffer);

		/*
		 * Write the current command to the proxy's statfile and keep
		 * a copy in `lastcmd'.
		 */

		copy_string(lastcmd, strcasecmp(command, "PASS") == 0? "PASS XXX": buffer, sizeof(lastcmd));
		snprintf (buffer, sizeof(buffer) - 2, "- %s", lastcmd);
		writestatfile(x, buffer);
		}


	if (sfgets(x, line, size) == NULL) {
		if (debug != 0)
			fprintf (stderr, "server disappered in sfputc(), pos #1\n");

		return (-1);
		}
	else if (strlen(line) < 3) {
		if (debug != 0)
			fprintf (stderr, "short server reply in sfputc()\n");

		return (-1);
		}

	rc = atoi(line);
	if (line[3] != ' '  &&  line[3] != 0) {
        	while (1) {
                	if (sfgets(x, line, size) == NULL) {
				printerror(ERR_SERVER | 1, "-ERR", "lost server while reading client greeting: %s", x->server.name);
				exit (1);
				}

			if (strlen(line) < 3)
				/* line too short to be response's last line */ ;
			else if (line[3] != ' '  &&  line[3] != 0)
				/* neither white space nor EOL at position #4 */ ;
			else if (line[0] >= '0'  &&  line[0] <= '9'  &&  atoi(line) == rc)
                       		break;		/* status code followed by EOL or blank detected */
			}
        	}

	if (here != NULL) {
		p = skip_ws(&line[3]);
		*here = p;
		}

	/*
	 * Update the statfile with the server's status response.
	 */

	snprintf (buffer, sizeof(buffer) - 2, "%3d %s", rc, lastcmd);
	writestatfile(x, buffer);

	return (rc);
}



int doquit(ftp_t *x)
{
	int	rc;
	char	resp[200];

	if ((rc = sfputc(x, "QUIT", "", resp, sizeof(resp), NULL)) != 221)
		printerror(0, "", "unexpected resonse to QUIT: %s", resp);

	cfputs(x, "221 goodbye");
	printerror(0, "", "%d QUIT", rc);
	
	return (0);
}


char * pasv_getipnum(char *line, char **here, char *ip, int size)
{
	int	c, i, k;

	copy_string(ip, line, size);
	k = 0;
	for (i=0; (c = ip[i]) != 0; i++) {
		if (c == ',') {
			if (k < 3) {
				ip[i] = '.';
				k++;
				}
			else {
				ip[i++] = 0;
				break;
				}
			}
		}

	if (here != NULL)
		*here = &line[i];

	return (ip);
}

unsigned long pasv_getport(char *line, char **here)
{
	unsigned long port;
	char	*p;

	p = line;
	port = strtoul(p, &p, 10);
	if (*p != ',')
		return (0);

	p++;
	port = (port << 8) + strtoul(p, &p, 10);
	if (here != NULL)
		*here = p;

	return (port);
}


int doport(ftp_t *x, char *command, char *par, int USE_EPRT)
{
	int	c, rc;
	char *p, line[200];
	dtc_t *ch = &x->ch;

	if (USE_EPRT) {
		// EPRT |1|127.0.0.1|58377|
		// only want the port |58377|, will connect to the stored client IP
		p = strrchr (par, '|');
		if (p) {
			*p = 0;
			p = strrchr (par, '|');
			if (p) {
				p++;
				ch->client.port = atoi (p);
			}
		}
		if (!p) {
			cfputs(x, "500 not accepted");
			return (0);
		}
		copy_string (ch->client.ipnum, x->client.ipnum, sizeof(ch->client.ipnum));
	} else {
		pasv_getipnum(par, &p, ch->client.ipnum, sizeof(ch->client.ipnum));
		ch->client.port = pasv_getport(p, &p);
	}
	if (debug != 0)
		fprintf (stderr, "client listens on %s:%u\n", ch->client.ipnum, ch->client.port);

	get_interface_info(x->fd.server, &ch->outside);
	ch->osock = bind_to_port(ch->outside.ipnum, 0);
	ch->outside.port = get_interface_info(ch->osock, &ch->outside);
	if (debug != 0)
		fprintf (stderr, "listening on %s:%u\n", ch->outside.ipnum, ch->outside.port);

	if (USE_EPRT) {
		// EPRT |X|ipstr|port|
		snprintf (line, sizeof(line), "|%c|%s|%d|", use_ipv6 ? '2' : '1',
		                              ch->outside.ipnum, ch->outside.port);
	} else {
		copy_string(line, ch->outside.ipnum, sizeof(line));
		for (p=line; (c = *p) != 0; p++) {
			if (c == '.')
				*p = ',';
			}
		*p++ = ',';
		snprintf (p, 20, "%u,%u", ch->outside.port >> 8, ch->outside.port & 0xFF);
	}

	/* Open port first */		
	ch->isock     = -1;
	ch->mode      = MODE_PORT;
	ch->state     = PORT_LISTEN;

	/* then send PORT cmd */
	if (USE_EPRT) {
		rc = sfputc(x, "EPRT", line, line, sizeof(line), &p);
	} else {
		rc = sfputc(x, "PORT", line, line, sizeof(line), &p);
	}

	/* check return code */
	if (rc != 200){
		cfputs(x, "500 not accepted");
		close_ch(x, &x->ch);
		}
	else 
		cfputs(x, "200 ok, port allocated");

	*ch->command = 0;
	return (rc);
}


int dopasv(ftp_t *x, char *command, char *par, int USE_EPSV)
{
	int	c, k, rc, resp, invalid_response = 0;
	char	*p, line[200];
	dtc_t	*ch;

	ch = &x->ch;
	if (USE_EPSV) {
		rc = sfputc (x, "EPSV", "", line, sizeof(line), &p);
	} else {
		rc = sfputc (x, "PASV", "", line, sizeof(line), &p);
	}
	if (rc != 227 && rc != 229) {
		cfputs(x, "500 not accepted");
		return (0);
	}

	if (USE_EPSV) {
		// need only port number
		// 229 Entering Extended Passive Mode (|||65113|)
		resp = -1;
		p = strchr (line + 4, '(');
		if (p) {
			resp = sscanf (p + 1, "|||%u|", &(ch->server.port));
		}
		if (!p || resp != 1) {
			invalid_response = 1;
		} else {
			copy_string (ch->server.ipnum, x->server.ipnum, sizeof(ch->server.ipnum));
		}
	} else {
		// PASV
		/* Ende der Port-Koordinaten im Server-Response suchen. */
		k = strlen(line);
		while (k > 0  &&  isdigit(line[k-1]) == 0) {
			k--;
		}
		if (isdigit(line[k-1])) {
			line[k--] = 0;
			while (k > 0  &&  (isdigit(line[k-1])  ||  line[k-1] == ','))
				k--;
			}
		/* line[k] sollte jetzt auf die erste Ziffer des PASV Response * zeigen. */
		if (isdigit(line[k]) == 0) {
			invalid_response = 1;
		} else {
			/* Auslesen der PASV IP-Nummer und des Ports. */
			p = &line[k];
			pasv_getipnum(p, &p, ch->server.ipnum, sizeof(ch->server.ipnum));
			ch->server.port = pasv_getport(p, &p);
		}
	}

	if (invalid_response) {
		printerror(0, "", "can't locate passive response: %s", line);
		cfputs(x, "500 not accepted");
		return (0);
	}

	if (debug != 0) {
		fprintf (stderr, "server listens on %s:%u\n", ch->server.ipnum, ch->server.port);
	}
	get_interface_info(0, &ch->inside);
	ch->isock = bind_to_port(ch->inside.ipnum, 0);
	ch->inside.port = get_interface_info(ch->isock, &ch->inside);
	if (debug != 0)
		fprintf (stderr, "listening on %s:%u\n", ch->inside.ipnum, ch->inside.port);

	if (USE_EPSV) {
		snprintf (line, sizeof(line) - 2, "229 Entering Extended Passive Mode (|||%u|)", ch->inside.port);
	} else { // PASV
		snprintf (line, sizeof(line) - 2, "227 Entering Passive Mode (%s,%u,%u)",
		          ch->inside.ipnum, ch->inside.port >> 8, ch->inside.port & 0xFF);
		for (p=line; (c = *p) != 0; p++) {
			if (c == '.') *p = ',';
		}
	}
	cfputs(x, line);
	ch->osock = -1;
	ch->mode  = MODE_PASSIVE;
	ch->state = PORT_LISTEN;

	*ch->command = 0;
	ch->operation = 0;

	return (rc);
}


int dofeat(ftp_t *x)
{
	/*
	 * Not so easy because we have to align with the server response. 
	 */

	int	rc;
	char	*p, word[80], serverfeature[80], line[300];
	static char *proxyfeatlist = "SIZE:MDTM:EPSV:EPRT";

	sfputs(x, "%s", "FEAT");
	if (sfgets(x, line, sizeof(line)) == NULL) {
		printerror(ERR_SERVER | 1, "-ERR", "monitor: server not responding");
		exit (1);
		}

	rc = atoi(line);
	if (rc != 211) {
		/* kein FEAT Support */ ;
		cfputs(x, "502 command not implemented");
		return (1);
		}


	cfputs(x, "211-feature list follows");
	while (1) {
		if (sfgets(x, line, sizeof(line)) == NULL) {
			printerror(ERR_SERVER | 1, "", "lost server in FEAT response");
			exit (1);
			}

		else if (isupper(*line) != 0) {

			/*
			 * Ok, this supports FTP server that simply are not
			 * really RFC2389 aware.  They send their feature list
			 * unindented.  2007-06-19/asg
			 */

			}

		else if (*line != ' ') { 

			/*
			 * RFC2389 specifies exactly one space in this
			 * multi-line response.  Nothing else.
			 */

			break;
			}


		/* Get feature from server response ...
		 */

		copy_string(serverfeature, line, sizeof(serverfeature));
		strupr(serverfeature);


		/* ... and compare it against our feature list
		 */


		p = proxyfeatlist;
		while (*get_quoted(&p, ':', word, sizeof(word)) != 0) {
			if (strcmp(word, serverfeature) == 0) {
				snprintf (line, sizeof(line) - 4, " %s", word);
				cfputs(x, line);
				break;
				}
			}
		}

	cfputs(x, "211 end");
	return (0);
}




int run_acp(ftp_t *x)
{
	int	rc, pid, pfd[2];
	char	line[300];
	
	if (*x->config->acp == 0)
		return (0);

	rc = 0;
	if (pipe(pfd) != 0) {
		printerror(ERR_SYSTEM | 1, "-ERR", "can't pipe: %s", strerror(errno));
		exit (1);
		}
	else if ((pid = fork()) < 0) {
		printerror(ERR_SYSTEM | 1, "-ERR", "can't fork acp: %s", strerror(errno));
		exit (1);
		}
	else if (pid == 0) {
		int	argc;
		char	*argv[32];

		close(0);		/* Das acp kann nicht vom client lesen. */
		dup2(pfd[1], 2);	/* stderr wird vom parent gelesen. */
		close(pfd[0]);

		setvar("CALLREASON", "acp");

		copy_string(line, x->config->acp, sizeof(line));
		argc = split(line, argv, ' ', 30);
		argv[argc] = NULL;
		execvp(argv[0], argv);

		printerror(ERR_CONFIG | 1, "-ERR", "can't exec acp %s: %s", argv[0], strerror(errno));
		exit (1);
		}
	else {
		int	len;
		char	message[300];

		close(pfd[1]);
		*message = 0;
		if ((len = read(pfd[0], message, sizeof(message) - 2)) < 0)
			len = 0;

		message[len] = 0;
		noctrl(message);
		close(pfd[0]);

		if (waitpid(pid, &rc, 0) < 0) {
			printerror(ERR_CONFIG | 1, "-ERR", "error while waiting for acp: %s", strerror(errno));
			exit (1);
			}

		rc = WIFEXITED(rc) != 0? WEXITSTATUS(rc): 1;
		if (*message == 0)
			copy_string(message, rc == 0? "access granted": "access denied", sizeof(message));

		if (rc == 0)
			printerror(0, "", "%s (rc= %d)", message, rc);
		else {
			printerror(ERR_ZEROEXITCODE | ERR_ACCESS, "-ERR", "%s (rc= %d)",
					message, rc);
			}
		}
		
	return (rc);
}

static char *getvarname(char **here, char *var, int size)
{
	int	c, k;

	size = size - 2;
	k = 0;
	while ((c = **here) != 0) {
		*here += 1;
		if (c == ' '  ||  c == '\t'  ||  c == '=')
			break;

		if (k < size)
			var[k++] = c;
		}

	var[k] = 0;
	strupr(var);
	*here = skip_ws(*here);

	return (var);
}

int run_ctp(ftp_t *x)
{
	int	rc, pid, pfd[2];
	char	line[300];
	FILE	*fp;
	
	if (*x->config->ctp == 0)
		return (0);

	rc = 0;
	if (pipe(pfd) != 0) {
		printerror(ERR_SYSTEM | 1, "-ERR", "can't pipe: %s", strerror(errno));
		exit (1);
		}
	else if ((pid = fork()) < 0) {
		printerror(ERR_SYSTEM | 1, "-ERR", "can't fork trp: %s", strerror(errno));
		exit (1);
		}
	else if (pid == 0) {
		int	argc;
		char	*argv[32];

		close(0);		/* Das trp kann nicht vom client lesen. */
		dup2(pfd[1], 1);	/* stdout wird vom parent gelesen. */
		close(pfd[0]);

		setvar("CALLREASON", "ctp");

		copy_string(line, x->config->ctp, sizeof(line));
		argc = split(line, argv, ' ', 30);
		argv[argc] = NULL;
		execvp(argv[0], argv);

		printerror(ERR_CONFIG | 1, "-ERR", "can't exec trp %s: %s",
			argv[0], strerror(errno));
		exit (1);
		}
	else {
		char	*p, var[80], line[300];

		close(pfd[1]);
		fp = fdopen(pfd[0], "r");
		while (fgets(line, sizeof(line), fp)) {
			p = skip_ws(noctrl(line));
			getvarname(&p, var, sizeof(var));

			if (strcmp(var, "SERVERNAME") == 0  ||  strcmp(var, "SERVER") == 0)
				copy_string(x->server.name, p, sizeof(x->server.name));
			else if (strcmp(var, "SERVERLOGIN") == 0  ||  strcmp(var, "LOGIN") == 0)
				copy_string(x->username, p, sizeof(x->username));
			else if (strcmp(var, "SERVERPASSWD") == 0  ||  strcmp(var, "PASSWD") == 0)
				copy_string(x->password, p, sizeof(x->password));
			else if (strcmp(var, "SERVERPORT") == 0  ||  strcmp(var, "PORT") == 0)
				x->server.port = atoi(p);

			/*
			 * Enable the trp to send error messages.
			 */

			else if (strcmp(var, "-ERR") == 0  ||  strcmp(var, "-ERR:") == 0) {
				printerror(1 | ERR_CONFIG, "-ERR", "%s", skip_ws(p));
				exit (1);
				}
			}

		fclose(fp);


		if (waitpid(pid, &rc, 0) < 0) {
			printerror(1 | ERR_CONFIG, "-ERR", "error while waiting for trp: %s", strerror(errno));
			exit (1);
			}

		rc = WIFEXITED(rc) != 0? WEXITSTATUS(rc): 1;
		if (rc != 0) {
			printerror(1 | ERR_CONFIG, "-ERR", "ctp signals error condition, rc= %d", rc);
			exit (1);
			}
		}

	return (rc);
}

int get_ftpdir(ftp_t *x)
{
	int	rc, len;
	char	*p, *start, line[300];
	static char *quotes = "'\"'`";

	sfputs(x, "%s", "PWD");
	if (sfgets(x, line, sizeof(line)) == NULL) {
		printerror(1 | ERR_SERVER, "", "monitor: server not responding");
		exit (1);
		}

	rc = strtol(line, &p, 10);
	if (rc != 257) {
		printerror(1 | ERR_SERVER, "", "monitor: PWD status: %d", rc);
		exit (1);
		}

	p = skip_ws(p);
	if (*p == 0) {
		printerror(1 | ERR_SERVER, "", "monitor: directory unset");
		exit (1);
		}


	if ((start = strchr(p, '/')) == NULL) {
		printerror(1 | ERR_SERVER, "", "monitor: can't find directory in string: %s", p);
		exit (1);
		}

	get_word(&start, x->cwd, sizeof(x->cwd));
	if ((len = strlen(x->cwd)) > 0  &&  strchr(quotes, x->cwd[len-1]) != NULL)
		x->cwd[len - 1] = 0;

	if (*x->cwd != '/') {
		printerror(1 | ERR_SERVER, "", "monitor: invalid path: %s", x->cwd);
		exit (1);
		}
		
	printerror(0, "", "cwd: %s", x->cwd);
	return (0);
}

int get_ftppath(ftp_t *x, char *path)
{
	int	i, k, n, m;
	char	cwp[200], ftpdir[200], pbuf[200];
	char	*part[DIR_MAXDEPTH+5], *dir[DIR_MAXDEPTH+5];

	/*
	 * Zuerst wird das aktuelle Verzeichnis (der ftppath) in seine
	 * Einzelteile zerlegt ...
	 */

	if (*path == '/') {

		/*
		 * ... Ausnahme: die path-Angabe ist absolut ...
		 */

		dir[0] = "";
		n = 1;
		}
	else {
		copy_string(ftpdir, x->cwd, sizeof(ftpdir));
		if (*ftpdir != 0  &&  strcmp(ftpdir, "/") != 0)
			n = split(ftpdir, part, '/', DIR_MAXDEPTH);
		else {
			dir[0] = "";
			n = 1;
			}
		}

	/*
	 * ... danach der path.  Die path Teile werden unter Beachtung
	 * der ueblichen Regeln an die Teile des aktuellen Verzeichnisses
	 * angehangen ...
	 */

	copy_string(pbuf, path, sizeof(pbuf));
	m = split(pbuf, dir, '/', 15);
	for (i=0; i<m; i++) {
		if (*dir[i] == 0)
			/* zwei aufeinander folgende `/' */ ;
		else if (strcmp(dir[i], ".") == 0)
			/* nichts */ ;
		else if (strcmp(dir[i], "..") == 0) {
			if (n > 1)
				n = n - 1;
			}
		else
			part[n++] = dir[i];

		if (n < 1  ||  n >= DIR_MAXDEPTH)
			return (1);		/* ungueltiges Verzeichnis */
		}

	/*
	 * ... und das Ergebnis wieder zusammengesetzt.
	 */

	if (n <= 1) {
		strcpy(cwp, "/");
		}
	else {
		k = 0;
		for (i=1; i<n; i++) {
			/* Bug alert, there it is: if ((k + strlen(part[i]) + 1 + 2) >= sizeof(dir)) */
			if ((k + strlen(part[i]) + 1 + 2) >= sizeof(cwp))
				return (1);		/* Name zu lang */
				
			cwp[k++] = '/';
			strcpy(&cwp[k], part[i]);
			k += strlen(&cwp[k]);
			}

		cwp[k] = 0;
		}

	/*
	 * Der normalisierte path auf das Objekt (Datei oder Verzeichnis,
	 * ist hier egal) ist fertig.
	 */

	copy_string(x->filepath, cwp, sizeof(x->filepath));
/* printerror(0, "+INFO", "cwd= %s, path= %s, filepath= %s, n= %d, m= %d", x->cwd, path, x->filepath, n, m); */

	return (0);
}

int run_ccp(ftp_t *x, char *cmd, char *par)
{
	int	rc, pid, pfd[2], lfd[2];
	char	message[300], line[300];

	/*
	 * Wenn kein ccp angegeben ist ist alles erlaubt.
	 */

	if (*x->config->ccp == 0)
		return (CCP_OK);


	/*
	 * Der Vorgang fuer ccp's ist fast gleich mit dem fuer acp's.
	 */

	rc = 0;
	if (pipe(pfd) != 0  ||  pipe(lfd)) {
		printerror(1 | ERR_SYSTEM, "-ERR", "can't pipe: %s", strerror(errno));
		exit (1);
		}
	else if ((pid = fork()) < 0) {
		printerror(1 | ERR_SYSTEM, "-ERR", "can't fork ccp: %s", strerror(errno));
		exit (1);
		}
	else if (pid == 0) {
		int	argc;
		char	*argv[32];

		dup2(pfd[1], 2);	/* stderr nach FTP Client */
		close(pfd[0]);

		dup2(lfd[1], 1);	/* stdout nach syslog */
		close(lfd[0]);

		close(0);

		setvar("COMMAND", cmd);
		setvar("PARAMETER", par);

		setvar("SESSION", x->session);
		snprintf (line, sizeof(line) - 2, "%d", x->ccpcoll);
		setvar("CCPCOLL", line);

		setvar("FTPHOME", x->home);
		setvar("FTPPATH", x->filepath);
		setvar("CALLREASON", "ccp");

		copy_string(line, x->config->ccp, sizeof(line));
		argc = split(line, argv, ' ', 30);
		argv[argc] = NULL;
		execvp(argv[0], argv);

		printerror(1 | ERR_CONFIG, "-ERR", "can't exec ccp %s: %s", argv[0], strerror(errno));
		exit (1);
		}
	else {
		int	len;

		/*
		 * Nicht gebrauchte fd's schliessen.
		 */

		close(pfd[1]);
		close(lfd[1]);


		/*
		 * syslog Meldung lesen und entsprechende pipe schliessen.
		 */

		*message = 0;
		if ((len = read(lfd[0], message, sizeof(message) - 2)) < 0)
			len = 0;

		message[len] = 0;
		noctrl(message);
		close(lfd[0]);

		if (*message != 0)
			printerror(0, "", "%s", message);



		/*
		 * Fehlermeldung lesen, pipe schliessen.
		 */

		*message = 0;
		if ((len = read(pfd[0], message, sizeof(message) - 2)) < 0)
			len = 0;

		message[len] = 0;
		noctrl(message);
		close(pfd[0]);


		/*
		 * return code holen.
		 */

		if (waitpid(pid, &rc, 0) < 0) {
			printerror(1 | ERR_CONFIG, "-ERR", "error while waiting for ccp: %s", strerror(errno));
			exit (1);
			}

		rc = WIFEXITED(rc) != 0? WEXITSTATUS(rc): 1;
		if (rc == 0)
			return (CCP_OK);

		if (*message == 0)
			copy_string(message, "permission denied", sizeof(message));
		}

	x->ccpcoll++;
	if (isdigit(*message))
		cfputs(x, message);
	else {
		snprintf (line, sizeof(line) - 2, "553 %s", message);
		cfputs(x, line);
		}

	return (CCP_ERROR);
}


	/*
	 * dologin() accepts now blanks with in and at the end of
	 * passwords - 22JAN02asg
	 */

int dologin(ftp_t *x)
{
	int	c, i, rc, isredirected;
	char	*p, word[80], line[300];
	struct addrinfo *hostp;
			
	while (1) {
		if (cfgets(x, line, sizeof(line)) == NULL)
			return (1);

		if (x->config->allow_passwdblanks == 0)
			p = noctrl(line);
		else {
			p = line;
			for (i=strlen(line)-1; i>=0; i--) {
				if ((c = line[i]) != '\n'  &&  c != '\r') {
					line[i+1] = 0;
					break;
					}
				}
			}

		get_word(&p, word, sizeof(word));
		strupr(word);
		if (strcmp(word, "USER") == 0) {
			get_word(&p, x->username, sizeof(x->username));

			setsessionvar("", "USER", "%s", x->username);
			cfputs(x, "331 password required");
			}
		else if (strcmp(word, "PASS") == 0) {
			if (*x->username == 0) {
				cfputs(x, "503 give USER first");
				continue;
				}

			if (x->config->allow_passwdblanks == 0)
				get_word(&p, x->password, sizeof(x->password)); 
			else
				copy_string(x->password, p, sizeof(x->password));

			setsessionvar("", "PASSWORD", "%s", x->password);
			break;
			}
		else if (strcmp(word, "QUIT") == 0) {
			cfputs(x, "221 goodbye");
			return (2);
			}
		else {
			cfputs(x, "530 login first");
			}
		}




        /*
	 * If this is a redirected connection take the server information
	 * from the original request.
	 */

	isredirected = 0;
	if (x->config->redirmode == (REDIR_FORWARD | REDIR_FORWARD_ONLY)  &&  *x->origdst.ipnum != 0) {
		snprintf (x->server.name, sizeof(x->server.name) - 2, "%s:%u",
			x->origdst.ipnum, x->origdst.port);

		setvar("ORIGDST_SERVER", x->origdst.ipnum);
		setnumvar("ORIGDST_PORT", x->origdst.port);

		isredirected = 1;	/* Set flag for below. */
		}

	else if (*x->config->ctp != 0) {

		/*
		 * We are extremly liberate here with server selection
		 * if we have a dynamic control program, we accept
		 * anything here -- 03APR04asg
		 */

		if ((p = strxchr(x->username, x->config->serverdelim, x->config->use_last_at)) == NULL)
			*x->server.name = 0;
		else if (1  ||  x->config->use_last_at == 0) {
			*p++ = 0;
			copy_string(x->server.name, p, sizeof(x->server.name));
			}
		}

	else if (x->config->selectserver == 0) {

		/*
		 * Allow 'user@hostname' usernames -- 2008-06-09/wzk
		 */

		if (x->config->use_last_at != 0)
			;
		else if ((p = strxchr(x->username, x->config->serverdelim, x->config->use_last_at)) != NULL) {
			cfputs(x, "500 service unavailable");
			printerror(1 | ERR_CLIENT, "-ERR", "hostname supplied: %s", p);
			exit (1);
			}

		copy_string(x->server.name, x->config->server, sizeof(x->server.name));
		}
	else {

		/*
		 * Normally we search for the first '@' so that the client can 
		 * not use "proxy hopping". The option "-u" can override
		 * this behaviour.
		 */

		if (1  ||  x->config->use_last_at == 0) {
			if ((p = strxchr(x->username, x->config->serverdelim, x->config->use_last_at)) == NULL) {
				cfputs(x, "500 service unavailable");
				printerror(1 | ERR_CLIENT, "-ERR", "missing hostname");
				exit (1);
				}
			}

		*p++ = 0;
		copy_string(x->server.name, p, sizeof(x->server.name));
		}



	setvar("USER", x->username);
	setvar("SERVER", x->server.name);


	/*
	 * Read proxy login and password if available.  Redirected
	 * connections pass untouched.
	 */

	if (isredirected == 0) {
		if ((p = strchr(x->username, ':')) != NULL) {
			*p++ = 0;
			copy_string(x->local.username, x->username, sizeof(x->local.username));
			copy_string(x->username, p, sizeof(x->username));

			setsessionvar("", "USER", "%s", x->username);
			}

		if ((p = strchr(x->password, ':')) != NULL) {
			*p++ = 0;
			copy_string(x->local.password, x->password, sizeof(x->local.password));
			copy_string(x->password, p, sizeof(x->password));
			}
		}


        /*
         * Call the dynamic configuration program.
         */

        if (*x->config->ctp != 0) {
		x->server.port = get_port(x->server.name, 21);

                if (run_ctp(x) != 0)
                        exit (0);       /* Never happens, we exit in run_ctp() */

		if (debug != 0) {
	                fprintf (stderr, "dcp debug: server= %s:%u, login= %s, passwd= %s",
					x->server.name, x->server.port,
					x->username, x->password);
			}

		setvar("USER", x->username);
		setvar("SERVER", x->server.name);
                }


	/*
	 * Get port and IP number of server.
	 */
	x->server.port = get_port(x->server.name, 21);
	hostp = lookup_host (x->server.name, NULL, x->server.port);
	if (hostp == NULL) {
		cfputs(x, "500 service unavailable");
		printerror(1 | ERR_PROXY, "-ERR", "can't resolve hostname: %s", x->server.name);
		exit (1);
		}
	w_sockaddr_get_ip_str (hostp->ai_addr, x->server.ipnum, sizeof(x->server.ipnum));
	freeaddrinfo (hostp);

	/*
	 * Call the access control program to check if the proxy
	 * request is allowed.  Moved code here -- 03APR04asg
	 */

	if (*x->config->acp != 0) {
		if (run_acp(x) != 0)
			exit (0);
		}


	/*
	 * Verification if the destination server is on the given list
	 * is done now here.
	 *
	 * Notice: Prior to this change you could give a fixed desination
	 * server as command line argument and a list of allowed server
	 * too.  Meaningless because the proxy didn't care when the `server
	 * selection' option wasn't turned on.  Now also the fixed server
	 * is checked against the list.
	 *
	 * I don't expect that this breaks an already running configuration
	 * because as said above this configuration was senseless in earlier
	 * proxy versions -- 030404asg
	 */

	if ((p = x->config->serverlist) != NULL  &&  *p != 0) {
		int	permitted;
		char	pattern[80];

		permitted = 0;
		while ((p = skip_ws(p)), *get_quoted(&p, ',', pattern, sizeof(pattern)) != 0) {
			noctrl(pattern);
			if (strpcmp(x->server.name, pattern) == 0) {
				permitted = 1;
				break;
				}
			}

		if (permitted == 0) {
			cfputs(x, "500 service unavailable");
			printerror(1 | ERR_ACCESS, "-ERR", "hostname not permitted: %s", x->server.name);
			exit (1);
			}
		}


	/*
	 * Establish connection to the server
	 */

	writestatfile(x, "LOGIN");
	if ((x->fd.server = openip(x->server.name, x->server.port, x->config->sourceip, 0)) < 0) {
		cfputs(x, "500 service unavailable");
		printerror(1 | ERR_CONNECT, "-ERR", "can't connect to server: %s", x->server.name);
		exit (1);
		}

	printerror(0, "", "connected to server: %s:%u", x->server.name, x->server.port);


	if (sfputc(x, NULL, NULL, line, sizeof(line), NULL) != 220) {
		cfputs(x, "500 service unavailable");
		printerror(1 | ERR_SERVER, "-ERR", "unexpected server greeting: %s", line);
		exit (1);
		}

	/*
	 * Login on FTP server.
	 *
	 * Complete rewrite because of servers wanting no password after
	 * login of anonymous user.
	 */

	rc = sfputc(x, "USER", x->username, line, sizeof(line), NULL);

	if (rc == 230) {
		cfputs(x, "230 login accepted");
		printerror(0, "", "login accepted: %s@%s, no password needed", x->username, x->server.name);
		return (0);
		}
	else if (rc != 331) {
		cfputs(x, "500 service unavailable");
		printerror(1 | ERR_SERVER, "-ERR", "unexpected reply to USER: %s", line);
		exit (1);
		}
	else if (sfputc(x, "PASS", x->password, line, sizeof(line), NULL) != 230) {
		cfputs(x, "530 bad login");
		printerror(1 | ERR_SERVER, "-ERR", "reply to PASS: %s", line);
		exit (1);
		}

	cfputs(x, "230 login accepted");
	printerror(0, "", "login accepted: %s@%s", x->username, x->server.name);

	return (0);
}



	/*
	 * dotransparentlogin() 
	 * only for transparent (redirected) connections
	 */

int dotransparentlogin(ftp_t *x)
{
	int	c, i, rc, rc2;
	char	*p, word[80], cline[300], sline[300];

        /*
	 * Check if we are called with incompatible parameters
	 */

        if (*x->config->ctp != 0)
		printerror(1 | ERR_CONFIG, "-ERR", "you can't use ctp in combination with transparent login so far!");

	else if ((p = x->config->serverlist) != NULL  &&  *p != 0)
		printerror(1 | ERR_CONFIG, "-ERR", "you can't use serverlist in combination with transparent login so far!");

	else if (*x->config->acp != 0)
		printerror(1 | ERR_CONFIG, "-ERR", "you can't use acp in combination with transparent login so far!");
		

        /*
	 * This is a redirected connection take the server information
	 * from the original request.
	 */

	if (x->config->redirmode == (REDIR_FORWARD | REDIR_FORWARD_ONLY)  &&  *x->origdst.ipnum != 0) {

		copy_string(x->server.name, x->origdst.ipnum, sizeof(x->server.name));
		x->server.port = x->origdst.port;

		setvar("ORIGDST_SERVER", x->origdst.ipnum);
		setnumvar("ORIGDST_PORT", x->origdst.port);

		}

	setvar("USER", x->username);
	setvar("SERVER", x->server.name);

	/*
	 * Establish connection to the server
	 */

	writestatfile(x, "LOGIN");
	if ((x->fd.server = openip(x->server.name, x->server.port, x->config->sourceip, 0)) < 0) {
		cfputs(x, "500 service unavailable");
		printerror(1 | ERR_CONNECT, "-ERR", "can't connect to server: %s", x->server.name);
		exit (1);
		}

	printerror(0, "", "connected to server: %s", x->server.name);


	rc = sfputc(x, NULL, NULL, sline, sizeof(sline), NULL);

	if (rc == 220) {
		cfputs(x, sline);
		while (1) {
			if (cfgets(x, cline, sizeof(cline)) == NULL)
				return (1);

			if (x->config->allow_passwdblanks == 0)
				p = noctrl(cline);
			else {
				p = cline;
				for (i=strlen(cline)-1; i>=0; i--) {
					if ((c = cline[i]) != '\n'  &&  c != '\r') {
						cline[i+1] = 0;
						break;
						}
					}
				}

			get_word(&p, word, sizeof(word));
			strupr(word);

			if (strcmp(word, "USER") == 0) {
				get_word(&p, x->username, sizeof(x->username));

				setsessionvar("", "USER", "%s", x->username);

				rc2 = sfputc(x, "USER", x->username, sline, sizeof(sline), NULL);

				if (rc2 == 230) {
					cfputs(x, sline);
					printerror(0, "", "login accepted: %s@%s, no password needed", x->username, x->server.name);
					return (0);
					}

				if (rc2 == 331) {
					cfputs(x, sline);
					}

				else {
					cfputs(x, sline);
					printerror(1 | ERR_SERVER, "-ERR", "unexpected reply to USER: %s", sline);
					exit (1);
					}
				}	

			else if (strcmp(word, "PASS") == 0) {
				if (*x->username == 0) {
					cfputs(x, "503 give USER first");
					continue;
					}

				if (x->config->allow_passwdblanks == 0)
					get_word(&p, x->password, sizeof(x->password)); 
				else
					copy_string(x->password, p, sizeof(x->password));

				setsessionvar("", "PASSWORD", "%s", x->password);

				rc2 = sfputc(x, "PASS", x->password, sline, sizeof(sline), NULL);

				if (rc2 != 230) {
					cfputs(x, sline);
					printerror(1 | ERR_SERVER, "-ERR", "reply to PASS: %s", sline);
					exit (1);
					}
				else {
					cfputs(x, sline);
					printerror(0, "", "login accepted: %s@%s", x->username, x->server.name);
					return (0);
					}
				}

			else if (strcmp(word, "QUIT") == 0) {
				cfputs(x, "221 goodbye");
				return (2);
				}
			else {
				cfputs(x, "530 login first");
				}
			}
		}

	else {
		cfputs(x, "500 service unavailable");
		printerror(1 | ERR_SERVER, "-ERR", "unexpected server greeting: %s", cline);
		exit (1);
		}

	return (0);
}


ftpcmd_t *getcmd(char *name)
{
	int	i;

	for (i=0; cmdtab[i].name[0] != 0; i++) {
		if (strcmp(cmdtab[i].name, name) == 0)
			return (&cmdtab[i]);
		}

	return (NULL);
}


int proxy_request(config_t *config)
{
	int	rc;
	char	*p, command[200], parameter[200], line[300];
	ftpcmd_t *cmd;
	ftp_t	*x;

	setnumvar("PID", getpid());
	setnumvar("STARTED", time(NULL));

	/*
	 * Set socket options to prevent us from the rare case that
	 * we transfer data to/from the client before the client has
	 * seen our "150 ..." message.
	 * Seems so that is doesn't work on all systems.
	 * So temporary only enable it on linux. 
	 */

#if defined(__linux__)

	rc = 1;
	if (setsockopt(1, SOL_TCP, TCP_NODELAY, &rc, sizeof(rc)) != 0)
		printerror(0, "", "can't set TCP_NODELAY, error= %s", strerror(errno));

#endif

	if (config->bsize <= 0)
		config->bsize = 1024;
	else if (config->bsize > FTPMAXBSIZE)
		config->bsize = FTPMAXBSIZE;

	x = allocate(sizeof(ftp_t));
	x->config = config;
	x->started = time(NULL);
	snprintf (x->session, sizeof(x->session) - 2, "%lu-%u", time(NULL), getpid());
	writestatfile(x, "STARTED");


	/*
	 * Fix potential problems after immediate initial unseccsesful
	 * up/downloads.  Wasn't a problem since we all do a LIST
	 * at first.
	 */

	x->ch.isock = -1;
	x->ch.osock = -1;

#ifdef FTP_FILECOPY
	x->ch.copyfd = -1;
#endif


	if (get_client_info(x, 0) < 0) {
		printerror(1 | ERR_SYSTEM, "-ERR", "can't get client info: %s", strerror(errno));
		exit (1);
		}

	setvar("CLIENTNAME", x->client.name);
	setvar("CLIENT", x->client.ipnum);

	get_interface_info(0, &x->i);
	printerror(0, "", "connected to client: %s, interface= %s:%u", x->client.name,
				x->i.ipnum, x->i.port);

	snprintf (line, sizeof(line) - 2, "%s:%u", x->i.ipnum, x->i.port);
	setvar("INTERFACE", line);


	if (*x->config->configfile != 0) {
		if (readconfig(x->config, x->config->configfile, x->i.ipnum) == 0) {
			cfputs(x, "421 not available");
			printerror(1 | ERR_CLIENT, "-ERR", "unconfigured interface: %s", x->i.ipnum);
			exit (1);
			}
		}

/*	printerror(0, "", "info: monitor mode: %s, ccp: %s",
 *			x->config->monitor == 0? "off": "on",
 *			*x->config->ccp == 0? "<unset>": x->config->ccp);
 */

#if defined (__linux__)

	/*
	 * Get redirection data if available.
	 */

	if (x->config->redirmode != 0) {
		int	rc;
		socklen_t socksize;
		struct sockaddr * sock = w_sockaddr_new (use_ipv6);
		char ipstr[80];
		unsigned short sport;

		socksize = w_sockaddr_get_size (sock);

		rc = getsockopt(0, use_ipv6 ? SOL_IPV6 : SOL_IP,
		                SO_ORIGINAL_DST, // IP6T_SO_ORIGINAL_DST has the same value
		                sock, &socksize);

		w_sockaddr_get_ip_str (sock, ipstr, sizeof(ipstr));
		sport = w_sockaddr_get_port (sock);
		free (sock);

		if (rc != 0)
			;
		else if (strcmp(ipstr,x->i.ipnum) != 0  || sport != x->i.port) {
			/*
			 * Take the original server information if it's
			 * a redirected request.
			 */
			copy_string(x->origdst.ipnum, ipstr, sizeof(x->origdst.ipnum));
			x->origdst.port = sport;
			setvar("ORIGDST_SERVER", x->origdst.ipnum);
			setnumvar("ORIGDST_PORT", x->origdst.port);

			printerror(0, "+INFO", "connection redirected, origdst: %s:%u", x->origdst.ipnum, x->origdst.port);
			}

		if (x->config->redirmode == REDIR_FORWARD_ONLY  &&  *x->origdst.ipnum == 0) {
			printerror(1, "-ERR", "session error, client= %s, error= connection not redirected",
					x->client.name);
			}
		}

#endif
	
        if (x->config->transparentlogin != 0) {
		if ((rc = dotransparentlogin(x)) < 0)
			return (1);
		else if (rc == 2)
			return (0);
		}
	else {
		cfputs(x, "220 server ready - login please");
		if ((rc = dologin(x)) < 0)
			return (1);
		else if (rc == 2)
			return (0);
		}



	/*
	 * Open the xferlog if we have one.
	 */

	if (*x->config->xferlog != 0) {
		if (*x->server.name == 0)
			copy_string(x->logusername, x->username, sizeof(x->logusername));
		else if (x->server.port != 21)
			snprintf (x->logusername, sizeof(x->logusername), "%s@%s:%u", x->username, x->server.name, x->server.port);
		else
			snprintf (x->logusername, sizeof(x->logusername), "%s@%s", x->username, x->server.name);

		x->xlfp = fopen(x->config->xferlog, "a");
		if (x->xlfp == NULL) {
			printerror(0, "-WARN", "can't open xferlog: %s, error= %s",
					x->config->xferlog, strerror(errno));
			}
		}

	if (x->config->monitor) {
		get_ftpdir(x);
		copy_string(x->home, x->cwd, sizeof(x->home));
		}

	writestatfile(x, "READY");
	printerror(0, "+INFO", "%s", getstatusline("connection ready"));

	while ((p = cfgets(x, line, sizeof(line))) != NULL) {
		if (*p == '\001') {
			if (*x->ch.command != 0) {
				struct timeval tn, d1, d2;

				gettimeofday(&tn, NULL);
				timersub(&tn, &x->ch.start1, &d1);
				timersub(&tn, &x->ch.start2, &d2);

				printerror(0, "", "%s %s, size= %ld bytes, t1= %lu.%06lu, td= %lu.%06lu",
						x->ch.command, x->ch.filename, x->ch.bytes,
						d1.tv_sec, d1.tv_usec,
						d2.tv_sec, d2.tv_usec);

				/*
				 * Update counter variables
				 */

				setnumvar("BYTES_STC", x->btc);
				setnumvar("BYTES_CTS", x->bts);

				/*
				 * Write xferlog if necessary.
				 */

				if (x->xlfp != NULL) {
					long	now;
					char	date[80];

					/*
					 * Write xferlog entry but notice that (1) sessions are never
					 * flagged as anonymous and (2) the transfer type is always
					 * binary (type flag was added to data channel but is
					 * actually not used. 10MAY04wzk
					 */

					now = time(NULL);
					copy_string(date, ctime(&now), sizeof(date));
					fprintf (x->xlfp, "%s %lu %s %lu %s %c %c %c %c %s %s %d %s %c\n",
							date,
							now - x->ch.start2.tv_sec,
							x->client.ipnum,
							x->ch.bytes,
							x->ch.filename,
							'b',		/* x->ch.type == TYPE_ASC? 'a': 'b', */
							'-',
							strcmp(x->ch.command, "RETR")? 'i': 'o',
							'u',		/* x->isanonymous == 1? 'a': 'u', */
							x->logusername,
							"ftp", 1, x->logusername, 'c');
					fflush(x->xlfp);
					}
				}

			/*
			 * Handle multiline server responses after the
			 * data transfer.
			 */

			sfputc(x, NULL, NULL, line, sizeof(line), NULL);
			cfputs(x, line);

			continue;
			}

		p = noctrl(line);
		get_word(&p, command, sizeof(command));
		strupr(command);

		if ((cmd = getcmd(command)) == NULL  ||  cmd->resp == -1) {
			cfputs(x, "502 command not implemented");
			printerror(0, "", "command not implemented: %s", command);
			continue;
			}

		*x->filepath = 0;
		if (cmd->par == 0)
			*parameter = 0;
		else {
			if (strcmp(command, "CDUP") == 0)
				strcpy(parameter, "..");
			else if (strcmp(command, "SITE") == 0)
				copy_string(parameter, p, sizeof(parameter));
			else {
				if (x->config->allow_blanks != 0)
					copy_string(parameter, p, sizeof(parameter));
				else
					get_word(&p, parameter, sizeof(parameter));
					
				if (*parameter == 0) {
					if (strcmp(command, "LIST") == 0  ||  strcmp(command, "NLST") == 0)
						/* nichts, ist ok */ ;
					else {
						printerror(1 | ERR_OTHER, "-ERR", "parameter required: %s", command);
						exit (1);
						}
					}
				}

			if (cmd->ispath != 0) {
				if (x->config->monitor) {
					if ((strcmp(command, "LIST") == 0  ||  strcmp(command, "NLST") == 0)
					    &&  *parameter == 0) {

						/*
						 * Sonderfall: wir simulieren `*' als Parameter.
						 */

						get_ftppath(x, "*");
						}
					else
						get_ftppath(x, parameter);
					}

				if (*x->filepath == 0)
					snprintf (x->filepath, sizeof(x->filepath) - 2, "%s", (*parameter == 0)? "*": parameter);
				}
			}


		if (cmd->useccp != 0) {
			if (run_ccp(x, command, parameter) != CCP_OK)
				continue;
			}


		if (strcmp(command, "QUIT") == 0) {
/*		        run_ccp(x, "QUIT", ""); */
			doquit(x);
			break;
			}
		else if (strcmp(command, "PORT") == 0)
			doport(x, command, parameter, 0);
		else if (strcmp(command, "EPRT") == 0)
			doport(x, command, parameter, 1);
		else if (strcmp(command, "FEAT") == 0)
			dofeat(x);
		else if (strcmp(command, "PASV") == 0)
			dopasv(x, command, parameter, 0);
		else if (strcmp(command, "EPSV") == 0)
			dopasv(x, command, parameter, 1);
		else if (strcmp(command, "LIST") == 0  ||  strcmp(command, "NLST") == 0) {
			x->ch.operation = OP_GET;	/* fuer PASV mode */
			rc = sfputc(x, command, parameter, line, sizeof(line), NULL);
			if (rc == 125  ||  rc == 150) {
				x->ch.operation = OP_GET;
				x->ch.seen150   = 1;
				if (debug >= 2)
					fprintf (stderr, "received 150 response\n");
				}
			else
				close_ch(x, &x->ch);

			cfputs(x, line);
			*x->ch.command = 0;
			}
		else if (strcmp(command, "RETR") == 0) {
			x->ch.operation = OP_GET;	/* fuer PASV mode */

#ifdef FTP_FILECOPY
			__init_filecopy();
#endif

			rc = sfputc(x, "RETR", parameter, line, sizeof(line), NULL);
			if (rc == 125  ||  rc == 150) {
				x->ch.operation = OP_GET;
				x->ch.seen150   = 1;
				if (debug >= 2)
					fprintf (stderr, "received 150 response\n");
				}
			else
				close_ch(x, &x->ch);


			cfputs(x, line);
			copy_string(x->ch.command, "RETR", sizeof(x->ch.command));
			copy_string(x->ch.filename, x->config->monitor != 0? x->filepath: parameter, sizeof(x->ch.filename));
			x->ch.bytes = 0;

#ifdef FTP_FILECOPY
			writeinfofile(x, line);
#endif

			if (extralog != 0)
				printerror(0, "", "%d RETR %s", rc, (0  &&  x->config->monitor != 0)? parameter: x->filepath);

			gettimeofday(&x->ch.start1, NULL);
			}
		else if (strcmp(command, "STOR") == 0  ||  strcmp(command, "APPE") == 0  ||  strcmp(command, "STOU") == 0) {
			x->ch.operation = OP_PUT;	/* fuer PASV mode */

#ifdef FTP_FILECOPY
			__init_filecopy();
#endif

			rc = sfputc(x, command, parameter, line, sizeof(line), NULL);
			if (rc == 125  ||  rc == 150) {
				x->ch.operation = OP_PUT;
				x->ch.seen150   = 1;

				if (debug >= 2)
					fprintf (stderr, "received 150 response\n");

				copy_string(x->ch.command, command, sizeof(x->ch.command));
				}
			else
				close_ch(x, &x->ch);

			cfputs(x, line);
			copy_string(x->ch.filename, x->config->monitor != 0? x->filepath: parameter, sizeof(x->ch.filename));
			if (extralog != 0) {
				if (strcmp(command, "STOU") == 0)
					printerror(0, "", "%d %s %s", rc, command, "-");
				else
					printerror(0, "", "%d %s %s", rc, command, x->ch.filename);
				}

			x->ch.bytes = 0;

#ifdef FTP_FILECOPY
			writeinfofile(x, line);
#endif

			gettimeofday(&x->ch.start1, NULL);
			}
		else {
			if (strcmp(command, "CDUP") == 0)
				*parameter = 0;

			rc = sfputc(x, command, parameter, line, sizeof(line), NULL);
			cfputs(x, line);
			if (extralog != 0  &&  cmd->log != 0) {
				if (x->config->monitor != 0  &&  cmd->ispath != 0)
					printerror(0, "", "%d %s %s", rc, command, x->filepath);
				else
					printerror(0, "", "%d %s%s%s", rc, command, *parameter != 0? " ": "", parameter);
				}

			if (strcmp(command, "CWD") == 0  ||  strcmp(command, "CDUP") == 0) {
				if (x->config->monitor)
					get_ftpdir(x);
				}
			}
		}

	if (*x->config->ccp != 0)
		run_ccp(x, "+EXIT", x->session);


	writestatfile(x, "QUIT");
	printerror(0, "+OK", "%s", getstatusline("ok"));

	if (*get_exithandler() != 0) {
		setnumvar("BYTES_STC", x->btc);
		setnumvar("BYTES_CTS", x->bts);
		run_exithandler(ERR_OK, "");
		}

	return (0);
}



