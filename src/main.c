
/*

    File: ftpproxy/main.c

    Copyright (C) 1999, 2000  Wolfgang Zekoll  <wzk@quietsche-entchen.de>
    Copyright (C) 2000, 2001  Andreas Schoenberg  <asg@compucation.de>
  
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

#include <signal.h>
#include <wait.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/time.h>

#include "ftp.h"
#include "ip-lib.h"
#include "lib.h"


char	*program =		"";
char	progname[80] =		"";

int	debug =			0;
int	extralog =		0;

int	uid =			-1;
int	gid =			-2;


void missing_arg(int c, char *string)
{
	fprintf (stderr, "%s: missing arg: -%c, %s\n", program, c, string);
	exit (-1);
}

int main(int argc, char *argv[], char *envp[])
{
	int	c, i, k;
	char	*p, option[80];
	config_t *config;
	

	if ((p = strrchr(argv[0], '/')) == NULL)
		program = argv[0];
	else {
		copy_string(progname, &p[1], sizeof(progname));
		program = progname;
		}

	config = allocate(sizeof(config_t));
	config->timeout = 15 * 60;
	strcpy(config->varname, "PROXY_");

	openlog(program, LOG_PID, LOG_MAIL);

	k = 1;
	while (k < argc  &&  argv[k][0] == '-'  &&  argv[k][1] != 0) {
		copy_string(option, argv[k++], sizeof(option));
		for (i=1; (c = option[i]) != 0; i++) {
			if (c == 'd')
				debug = 1;
			else if (c == 'a') {
				if (k >= argc)
					missing_arg(c, "access control program");

				copy_string(config->acp, argv[k++], sizeof(config->acp));
				}
			else if (c == 'b')
				config->allow_blanks = 1;
			else if (c == 'c') {
				if (k >= argc)
					missing_arg(c, "command control program");

				copy_string(config->ccp, argv[k++], sizeof(config->ccp));
				}
			else if (c == 'e')
				config->selectserver = 1;
			else if (c == 'l')
				extralog = 1;
			else if (c == 'm')
				config->monitor = 1;
			else if (c == 'n')
				config->numeric_only = 1;
			else if (c == 'p') {
				if (k >= argc)
					missing_arg(c, "data port");

				config->dataport = strtoul(argv[k++], NULL, 10);
				if (config->dataport == 0)
					config->dataport = 20;
				}
			else if (c == 's') {
				if (k >= argc)
					missing_arg(c, "server list");

				config->u.serverlist = argv[k++];
				}
			else if (c == 't') {
				if (k >= argc)
					missing_arg(c, "timeout");

				config->timeout = atoi(argv[k++]);
				if (config->timeout < 1)
					config->timeout = 60;
				}
			else if (c == 'v') {
				if (k >= argc)
					missing_arg(c, "varname prefix");

				copy_string(config->varname, argv[k++], sizeof(config->varname));
				}
/*			else if (c == 'y')
 *				x->cleanenv = 1;
 */
 			else if (c == 'z') {
				if (k >= argc)
					missing_arg(c, "buffer size");

				config->bsize = atoi(argv[k++]);
				}
			else {
				fprintf (stderr, "%s: unknown option: -%c\n", program, c);
				exit (-1);
				}
			}
		}


	if (config->selectserver == 0) {
		if (k >= argc) {
			fprintf (stderr, "usage: %s [<options>] <server>\n", program);
			exit (1);
			}

		copy_string(config->u.server, argv[k++], sizeof(config->u.server));
		}
	
	if (k < argc) {
		fprintf (stderr, "%s: extra arguments on command line: %s ...\n", program, argv[k]);
		exit (1);
		}
		
	proxy_request(config);
	exit (0);
}


