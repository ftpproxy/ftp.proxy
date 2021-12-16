
/*

    File: ftpproxy/ip-lib.c

    Copyright (C) 1999,2005  Wolfgang Zekoll  <wzk@quietsche-entchen.de>
    Copyright (C) 2000 - 2006  Andreas Schoenberg  <asg@ftpproxy.org>
  
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
#include <ctype.h>

#include <signal.h>
#include <syslog.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include "ftp.h"
#include "procinfo.h"
#include "lib.h"
#include "ip-lib.h"

int use_ipv6 = 0;

// =============================================================================

struct sockaddr * w_sockaddr_new (int ipv6) // must be freed with free()
{
    struct sockaddr * saddr;
    if (ipv6) {
        saddr = calloc (1, sizeof(struct sockaddr_in6));
        saddr->sa_family = AF_INET6;
    } else {
        saddr = calloc (1, sizeof(struct sockaddr_in));
        saddr->sa_family = AF_INET;
    }
    return saddr;
}


int w_sockaddr_get_port (struct sockaddr * saddr)
{
    unsigned short sin_port;
    if (saddr->sa_family == AF_INET) {
        sin_port = (((struct sockaddr_in*)saddr)->sin_port);
    } else { /* ipv6 */
        sin_port = (((struct sockaddr_in6*)saddr)->sin6_port);
    }
    return (ntohs (sin_port));
}


void w_sockaddr_get_ip_str (struct sockaddr * saddr, char * outbuf, int size)
{
    void * sin_addr;
    *outbuf = 0;
    if (saddr->sa_family == AF_INET) {
        sin_addr = &(((struct sockaddr_in*)saddr)->sin_addr);
    } else { /* ipv6 */
        sin_addr = &(((struct sockaddr_in6*)saddr)->sin6_addr);
    }
    inet_ntop (saddr->sa_family, sin_addr, outbuf, size);
}


void * w_sockaddr_get_addr (struct sockaddr * saddr)
{
    void * sin_addr;
    if (saddr->sa_family == AF_INET) {
        sin_addr = &(((struct sockaddr_in*)saddr)->sin_addr);
    } else { /* ipv6 */
        sin_addr = &(((struct sockaddr_in6*)saddr)->sin6_addr);
    }
    return sin_addr;
}


socklen_t w_sockaddr_get_size (struct sockaddr * saddr)
{
    if (saddr->sa_family == AF_INET) {
        return sizeof(struct sockaddr_in);
    } else { /* ipv6 */
        return sizeof(struct sockaddr_in6);
    }
}


void w_sockaddr_reset (struct sockaddr * saddr)
{
    if (saddr->sa_family == AF_INET) {
        memset (saddr, 0, sizeof(struct sockaddr_in));
        saddr->sa_family = AF_INET;
    } else { /* ipv6 */
        memset (saddr, 0, sizeof(struct sockaddr_in6));
        saddr->sa_family = AF_INET6;
    }
}


void w_sockaddr_set_port (struct sockaddr * saddr, int port)
{
    if (saddr->sa_family == AF_INET) {
        ((struct sockaddr_in*)saddr)->sin_port = htons (port);
    } else { /* ipv6 */
        ((struct sockaddr_in6*)saddr)->sin6_port = htons (port);
    }
}


int w_sockaddr_set_ip_from_str (struct sockaddr * saddr, const char * ipstr)
{
    int ret;
    void * addr = w_sockaddr_get_addr (saddr);
    ret = inet_pton (saddr->sa_family, ipstr, addr);
    return ret; // 1=ok
}

// =============================================================================

struct addrinfo * lookup_host (char * host, char * service, unsigned int port)
{
	// this is where everything about the connection is specified
	// the resulting addrinfo will contain info according to the params specified here
	struct addrinfo hints;
	struct addrinfo *hostp;
	char port_s[10];

	memset (&hints, 0, sizeof(hints));
	hints.ai_flags    = AI_CANONNAME;

	if (use_ipv6) {
		hints.ai_family = AF_INET6;
	} else {
		hints.ai_family = AF_INET;
	}
	hints.ai_socktype = SOCK_STREAM;

    if (port || (!service || !*service))
		snprintf (port_s, sizeof(port_s), "%u", port);
	else
		snprintf (port_s, sizeof(port_s), "%s", service);

	if (getaddrinfo (host, port_s, &hints, &hostp) != 0) {
		return NULL;
	}

	return hostp;
}

// =============================================================================

unsigned int get_interface_info(int pfd, peer_t *sock)
{
	unsigned int size;
	struct sockaddr * saddr = w_sockaddr_new (use_ipv6);

	size = w_sockaddr_get_size (saddr);
	if (getsockname(pfd, saddr, &size) < 0)
		printerror(1 | ERR_SYSTEM, "-ERR", "can't get sockname, error= %s", strerror(errno));

	w_sockaddr_get_ip_str (saddr, sock->ipnum, sizeof(sock->ipnum));
	sock->port = w_sockaddr_get_port (saddr);
	copy_string(sock->name, sock->ipnum, sizeof(sock->name));

	free (saddr);
	return (sock->port);
}


static void alarm_handler()
{
	return;
}


int openip(char *host, unsigned int port, char *srcip, unsigned int srcport)
{
	int	socketd;
	struct addrinfo *hostp, *bhostp;

	hostp = lookup_host (host, NULL, port);
	if (!hostp) {
		return -1;
	}

	socketd = socket (hostp->ai_family, SOCK_STREAM, 0);
	if (socketd < 0) {
		freeaddrinfo (hostp);
		return (-1);
	}

	if (srcip != NULL  &&  *srcip != 0)
	{
		/* Enhancement to use a particular local interface and source port,
		 * mentioned by Juergen Ilse, <ilse@asys-h.de>. */
		if (srcport != 0) {
			int	one;
			one = 1;
	 		setsockopt (socketd, SOL_SOCKET, SO_REUSEADDR, (int *) &one, sizeof(one));
		}
 
 		/* Bind local socket to srcport and srcip */
		bhostp = lookup_host (srcip, NULL, srcport);
		if (!bhostp) {
			printerror(1 | ERR_SYSTEM, "-ERR", "can't lookup %s", srcip);
			freeaddrinfo (hostp);
			exit (1);
		}

		if (bind(socketd, bhostp->ai_addr, bhostp->ai_addrlen)) {
			printerror(1 | ERR_SYSTEM, "-ERR", "can't bind to %s:%u", srcip, srcport);
			freeaddrinfo (hostp);
			freeaddrinfo (bhostp);
			exit (1);
		}
		freeaddrinfo (bhostp);
	}

	signal(SIGALRM, alarm_handler);
	alarm(10);

	if (connect(socketd, hostp->ai_addr, hostp->ai_addrlen) < 0) {
		freeaddrinfo (hostp);
		return -1;
	}
	freeaddrinfo (hostp);

	alarm(0);
	signal(SIGALRM, SIG_DFL);
	
 	return (socketd);
}

unsigned int getportnum(char *name)
{
	unsigned int port;
	struct servent *portdesc;
	
	if (isdigit(*name) != 0)
		port = atol(name);
	else {
		portdesc = getservbyname(name, "tcp");
		if (portdesc == NULL) {
			printerror(1 | ERR_SYSTEM, "-ERR", "service not found: %s", name);
			exit (1);
		}

		port = ntohs(portdesc->s_port);
		if (port == 0) {
			printerror(1 | ERR_SYSTEM, "-ERR", "port error: %s\n", name);
			exit (1);
		}
	}
	
	return (port);
}

unsigned int get_port(char *server, unsigned int def_port)
{
	unsigned int port;
	char	*p;

	if (use_ipv6)
		return def_port;

	if ((p = strchr(server, ':')) == NULL)
		return (def_port);

	*p++ = 0;
	port = getportnum(p);

	return (port);
}

int bind_to_port(char *interface, unsigned int port)
{
	struct sockaddr * saddr = w_sockaddr_new (use_ipv6);
	int	sock;
	struct addrinfo * ifp;

	sock = socket (saddr->sa_family, SOCK_STREAM, 0);
	if (sock < 0) {
		printerror(1 | ERR_SYSTEM, "-ERR", "can't create socket: %s", strerror(errno));
		free (saddr);
		exit (1);
	} 
	else {
		int	opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}

	if (interface && *interface)
	{
		ifp = lookup_host (interface, NULL, port);
		if (ifp == NULL) {
			printerror(1 | ERR_SYSTEM, "-ERR", "can't lookup %s", interface);
			free (saddr);
			exit (1);
		}
		memcpy (saddr, ifp->ai_addr, w_sockaddr_get_size(saddr));
		freeaddrinfo (ifp);
	}
	w_sockaddr_set_port (saddr, port);

	if (bind (sock, saddr, w_sockaddr_get_size(saddr))) {
		printerror(1 | ERR_SYSTEM, "-ERR", "can't bind to %s:%u", interface, port);
		free (saddr);
		exit (1);
	}

	if (listen(sock, 5) < 0) {
		printerror(1 | ERR_SYSTEM, "-ERR", "listen error: %s", strerror(errno));
		free (saddr);
		exit (1);
	}

	free (saddr);
	return (sock);
}

