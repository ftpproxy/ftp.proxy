
/*

    File: ftpproxy/filecopy.c 

    Copyright (C) 2009  Wolfgang Zekoll  <wzk@quietsche-entchen.de> 
  
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
#include <time.h>

#include "ftp.h"
#include "procinfo.h"
#include "lib.h"


#ifdef FTP_FILECOPY

	/* filecopy 5. - Error handling function: print error and terminate if configured. */

int dofilecopyerror(ftp_t *x, int error, char *par)
{
	int	rc;
	char	*tag;

	x->config->cp.errormode = FCEM_TERMINATE;

	rc = 0;
	tag = "-INFO";
	if (x->config->cp.errormode == FCEM_TERMINATE) {
		rc = 1;
		tag = "-ERR";
		}

	switch (error) {
	case FCE_CREATEDIR:
		printerror(rc, tag, "directory creation error, dir= %s, error= %s",
				par, strerror(errno));

	case FCE_CREATEDATA:
		printerror(rc, tag, "can't write or create file, filename= %s, error= %s",
				par, strerror(errno));

	case FCE_CREATEINFO:
		printerror(rc, tag, "can't write or create infofile, filename= %s, error= %s",
				par, strerror(errno));

	default:
		printerror(rc, tag, "filecopy error: %s", par);
		}

	return (0);
}


int initfilecopy(ftp_t *x, char *op, char *filename)
{
	int	c, k;
	char	*p, subdir[30], date[80], dir[200], efn[100];
	struct tm tm;
	struct stat sbuf;

	x->cp.create = 0;
	x->cp.count++;
	x->ch.copyfd = -1;

	x->cp.started = time(NULL);
	tm = *localtime((time_t *) &x->cp.started);
	strftime(subdir, sizeof(subdir) - 2, x->config->cp.subdir, &tm);

	copy_string(dir, x->config->cp.basedir, sizeof(dir));
	k = strlen(dir);
	p = subdir;
	while ((c = *p) != 0) {
		dir[k++] = '/';
		while ((c = *p++) != 0  &&  c != '/'  &&  (k < sizeof(dir) - 10))
			dir[k++] = c;

		dir[k] = 0;
		if (k > sizeof(dir) - 10)
			printerror(1, "-ERR", "path too long in initfilecopy(), dir= %s", dir);

		if (stat(dir, &sbuf) != 0  ||  S_ISDIR(sbuf.st_mode) == 0) {
			if (mkdir(dir, 0775) != 0) {
				dofilecopyerror(x, FCE_CREATEDIR, dir);
				return (1);
				}
			}

		if (c == 0)
			break;
		}


	if ((p = strrchr(filename, '/')) == NULL)
		p = filename;
	else
		p++;

	copy_string(x->cp.basename, p, sizeof(x->cp.basename) - 2);
	k = 0;
	while ((c = *p++) != 0) {
		if (k > sizeof(efn) - 10) {
			efn[k] = 0;
			printerror(1, "-ERR", "filename too long in initfilecopy(), filename= %s", efn);
			}

		if (isalnum(c) != 0  ||  strchr("_-.+", c) != NULL)
			efn[k++] = c;
		else {
			snprintf (&efn[k], 5, "%c%02X", '%', c);
			k += 3;
			}
		}

	efn[k] = 0;
	strftime(date, sizeof(date) - 2, "%Y-%m-%d+%H:%M:%S", &tm);
	snprintf (x->cp.filename, sizeof(x->cp.filename) - 2, "%s/%s,P%u,N%d,F%s.data",
			dir, date, getpid(), x->cp.count, efn);
	snprintf (x->cp.infofile, sizeof(x->cp.infofile) - 2, "%s/%s,P%u,N%d,F%s.info",
			dir, date, getpid(), x->cp.count, efn);

	if ((x->ch.copyfd = open(x->cp.filename, O_WRONLY | O_CREAT, 0644)) < 0) {
		dofilecopyerror(x, FCE_CREATEDATA, x->cp.filename);
		return (1);
		}

	if (writeinfofile(x, "000 Initialization done") != 0) {
		dofilecopyerror(x, FCE_CREATEINFO, x->cp.infofile);
		return (1);
		}

	x->cp.create = 1;
	return (0);
}


int writeinfofile(ftp_t *x, char *ftpstatus)
{
	char	date[80];
	struct tm tm;
	FILE	*fp;

	if (x->cp.create == 0)
		return (0);

	if ((fp = fopen(x->cp.infofile, "w")) == NULL) {
		dofilecopyerror(x, FCE_CREATEINFO, x->cp.infofile);
		return (1);
		}

	tm = *localtime((time_t *) &x->cp.started);
	strftime(date, sizeof(date) - 2, "%Y-%m-%d %H:%M:%S", &tm);

	fprintf (fp, "date: %s %ld %u\n", date, x->cp.started, getpid());
	fprintf (fp, "server: %s:%d\n", x->server.name, x->server.port);
	fprintf (fp, "client: %s\n", x->client.name);
	fprintf (fp, "username: %s\n", x->username);
	fprintf (fp, "filename: %s\n", x->cp.basename);
	fprintf (fp, "operation: %s\n", x->ch.command);
	fprintf (fp, "path: %s\n", x->ch.filename);
	fprintf (fp, "size: %ld\n", x->ch.bytes);
	fprintf (fp, "status: %s\n", ftpstatus);
	fclose (fp);

	return (0);
}

#endif

