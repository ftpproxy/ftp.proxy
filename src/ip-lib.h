
/*

    File: ftpproxy/ip-lib.h

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

#ifndef _IP_LIB_INCLUDED
#define	_IP_LIB_INCLUDED

extern char *program;
extern int use_ipv6;

typedef struct _peer {
	char        name[100];
	char        ipnum[64];
	unsigned int port;
	} peer_t;

struct sockaddr * w_sockaddr_new (int ipv6); // must be freed with free()
int  w_sockaddr_get_port   (struct sockaddr * saddr);
void w_sockaddr_get_ip_str (struct sockaddr * saddr, char * outbuf, int size);
void * w_sockaddr_get_addr (struct sockaddr * saddr);
socklen_t w_sockaddr_get_size (struct sockaddr * saddr);
void w_sockaddr_reset (struct sockaddr * saddr);
void w_sockaddr_set_port (struct sockaddr * saddr, int port);
int w_sockaddr_set_ip_from_str (struct sockaddr * saddr, const char * ipstr);

unsigned int get_interface_info(int pfd, peer_t *sock);

int openip(char *server, unsigned int port, char *srcip, unsigned int srcport);

unsigned int getportnum(char *name);
unsigned int get_port(char *server, unsigned int def_port);

int bind_to_port(char *interface, unsigned int port);
int accept_loop(int sock);
struct addrinfo * lookup_host (char * host, char * service, unsigned int port);

#endif
