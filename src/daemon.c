
/*

    File: ftpproxy/daemon.c 

    Copyright (C) 2002  Andreas Schoenberg  <asg@ftpproxy.org> 
  
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
#include <ctype.h>

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <pwd.h>

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


/* --------------------------------------------------------------------

static int get_yesno(char **from, char *par, char *filename, int lineno)
{
	char	word[80];

	if (**from == 0) {
		fprintf (stderr, "%s: missing parameter: %s, %s:%d\n", program, par, filename, lineno);
		exit (1);
		}

	get_word(from, word, sizeof(word));
	if (strcmp(word, "yes") == 0)
		return (1);
	else if (strcmp(word, "no") == 0)
		return (0);

	fprintf (stderr, "%s: bad parameter value: %s, parameter= %s, %s:%d\n", program, word, par, filename, lineno);
	exit (1);
	
	return (0);
}

static char *get_parameter(char **from, char *par, char *value, int size,
		char *filename, int lineno)
{
	if (**from == 0) {
		fprintf (stderr, "%s: missing parameter: %s, %s:%d\n", program, par, filename, lineno);
		exit (1);
		}

	copy_string(value, *from, size);
	return (value);
}

int readconfig(config_t *config, char *filename)
{
	int	lineno;
	char	*p, word[80], line[300];
	FILE	*fp;

	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf (stderr, "%s: can't open configuration file: %s\n", program, filename);
		exit (1);
		}

	lineno = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		lineno++;
		if ((p = strchr(line, '#')) != NULL)
			*p = 0;

		p = skip_ws(noctrl(line));
		if (*p == 0)
			continue;

		get_word(&p, word, sizeof(word));
		strlwr(word);
		p = skip_ws(p);

		if (strcmp(word, "debug") == 0)
			debug = get_yesno(&p, word, filename, lineno);
		
		else if (strcmp(word, "acp") == 0)
			get_parameter(&p, word, config->acp, sizeof(config->acp), filename, lineno);
		else if (strcmp(word, "ccp") == 0)
			get_parameter(&p, word, config->ccp, sizeof(config->ccp), filename, lineno);
		else if (strcmp(word, "handler") == 0)
			get_parameter(&p, word, config->handler, sizeof(config->handler), filename, lineno);

		else if (strcmp(word, "site") == 0  ||  strcmp(word, "site-handler") == 0)
			get_parameter(&p, word, config->sitehandler, sizeof(config->sitehandler), filename, lineno);

		else if (strcmp(word, "rootdir") == 0)
			get_parameter(&p, word, config->vroot, sizeof(config->vroot), filename, lineno);
		else if (strcmp(word, "homedir") == 0)
			get_parameter(&p, word, config->homedir, sizeof(config->homedir), filename, lineno);

		else if (strcmp(word, "allow-blanks") == 0)
			config->allow_blanks = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "allow-anon") == 0  ||  strcmp(word, "allow-anonymous") == 0)
			config->allow_anon = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "anon-user") == 0)
			get_parameter(&p, word, config->anon_user, sizeof(config->anon_user), filename, lineno);

		else if (strcmp(word, "setenv") == 0  ||  strcmp(word, "varname") == 0)
			get_parameter(&p, word, config->varname, sizeof(config->varname), filename, lineno);
		else if (strcmp(word, "denylist") == 0)
			get_parameter(&p, word, config->denylist, sizeof(config->denylist), filename, lineno);
		else if (strcmp(word, "timeout") == 0) {
			char	val[20];

			get_parameter(&p, word, val, sizeof(val), filename, lineno);
			config->timeout = strtoul(val, NULL, 8);
			if (config->timeout < 60)
				config->timeout = 60;
			}
		else if (strcmp(word, "umask") == 0) {
			char	mask[20];

			get_parameter(&p, word, mask, sizeof(mask), filename, lineno);
			config->umask = strtoul(mask, NULL, 8);
			}

		else if (strcmp(word, "user") == 0) {
			if (isalpha(*p)) {
				struct passwd *pw;

				if ((pw = getpwnam(p)) == NULL) {
					fprintf (stderr, "%s: no such user: %s, %s:%d\n", program, p, filename, lineno);
					exit (1);
					}

				config->user.uid = pw->pw_uid;
				config->user.gid = pw->pw_gid;
				}
			else {
				char	*q;

				if (*p == '.') {
					config->user.uid = 65535U;
					q = p;
					}
				else
					config->user.uid = strtoul(p, &q, 10);

				if (*q == 0)
					config->user.gid = 65534U;
				else if (*q == '.') {
					q++;
					config->user.gid = strtoul(q, &q, 10);
					}
				else {
					fprintf (stderr, "%s: error in user id: %s, %s:%d\n", program, p, filename, lineno);
					exit (1);
					}
				}
			}
		else {
			fprintf (stderr, "%s: unknown parameter: %s, %s:%d\n", program, word, filename, lineno);
			exit (1);
			}
		}
		
	fclose (fp);
	return (0);
}

   -------------------------------------------------------------------- */



int acceptloop(int sock)
{
	int	connect, pid, len;
	struct sockaddr_in client;

	/*
	 * Go into background.
	 */

	if ((pid = fork()) > 0)
		exit (1);

	fprintf (stderr, "\nstarting %s in daemon mode ...\n", version);
	while (1) {

		/*
		 * hier kommt ein accept an
		 */

		len = sizeof(client);
		if ((connect = accept(sock, (struct sockaddr *) &client, &len)) < 0) {
			if (errno == EINTR  ||  errno == ECONNABORTED)
				continue;

			fprintf (stderr, "%04X: accept error: %s\n", getpid(), strerror(errno));
			continue;
			}

		if ((pid = fork()) < 0) {
			fprintf (stderr, "%04X: can't fork process: %s\n", getpid(), strerror(errno));
			exit (1);
			}
		else if (pid == 0) {
			int optlen;
			struct linger linger;

			linger.l_onoff = 1;
			linger.l_linger = 2;
			optlen = sizeof(linger);
			if (setsockopt(connect, SOL_SOCKET, SO_LINGER, &linger, optlen) != 0)
				fprintf (stderr, "%04X: can't set linger\n", getpid());

			dup2(connect, 0);
			dup2(connect, 1);

			close (connect);
			close (sock);

			return (0);
			}

		/*
		 * Der folgende Teil wird nur im parent Prozess ausgefuehrt.
		 */

		close(connect);
		}

	close (1);
	fprintf (stderr, "%04X: terminating\n", getpid());

	exit (0);
}

