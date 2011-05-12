
/*

    File: procinfo.c

    Copyright (C) 2005,2006  Wolfgang Zekoll  <wzk@quietsche-entchen.de>
  
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

#include <ctype.h>
#include <signal.h>
#include <wait.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <syslog.h>
#include <sys/time.h>



#include "lib.h"
#include "ip-lib.h"
#include "ftp.h"
#include "procinfo.h"

extern int debug;

char	varprefix[40] =		"FTP_";

char	statdir[200] =		"";
char	sessiondir[200] =	"";

procinfo_t pi;

struct _errortype {
    int		num;
    char	name[20];
    } errortype[] = {
	{ ERR_ACCESS,	"ACCESS" },
	{ ERR_CONNECT,	"CONNECT" },
	{ ERR_TIMEOUT,	"TIMEOUT" },
	{ ERR_SERVER,	"SERVER" },
	{ ERR_CLIENT,	"CLIENT" },
	{ ERR_PROXY,	"PROXY" },
	{ ERR_CONFIG,	"CONFIG" },
	{ ERR_SYSTEM,	"SYSTEM" },
	{ ERR_OK,	"OK" },
	{ ERR_ANY,	"ANY" },
	{ ERR_ANY,	"ALL" },
	{ 1,		"NONE" },
	{ 0, "" }
    };


	/*
	 * Information functions: can be used (but are not -- not yet) to
	 * encapsulate variables in procinfo.c.
	 */

char *getpidfile()
{
	return (pi.pidfile);
}

char *getstatdir()
{
	return (statdir);
}



	/*
	 * Set variables.
	 */

char *setpidfile(char *pidfile)
{
	if (pidfile == NULL  ||  *pidfile == 0)
		snprintf (pi.pidfile, sizeof(pi.pidfile) - 2, "/var/run/%s.pid", program);
	else
		copy_string(pi.pidfile, pidfile, sizeof(pi.pidfile));

	return (pi.pidfile);
}

char *setstatdir(char *dir)
{
	copy_string(statdir, dir, sizeof(statdir));
	return (statdir);
}


	/*
	 * The following function do "the real work".
	 */

int init_procinfo(char *vp)
{
	memset(&pi, 0, sizeof(procinfo_t));
	if (vp != NULL)
		copy_string(varprefix, vp, sizeof(varprefix));

	atexit(exithandler);
	return (0);
}

FILE *getstatfp(void)
{
	if (*statdir == 0)
		return (NULL);

	if (*pi.statfile == 0) {
		snprintf(pi.statfile, sizeof(pi.statfile) - 2, "%s/%s-%05d.stat",
				statdir, program, getpid());
		if ((pi.statfp = fopen(pi.statfile, "w")) == NULL) {
			printerror(0, "-INFO", "can't open statfile %s, error= %s",
					pi.statfile, strerror(errno));
			}
		}

	return (pi.statfp);
}


	/*
	 * Exithandler
	 */

char *get_exithandler(void)
{
	return (pi.exithandler);
}

char *set_exithandler(char *handler)
{
	if (handler != NULL)
		copy_string(pi.exithandler, handler, sizeof(pi.exithandler));

	return (pi.exithandler);
}

int run_exithandler(int error, char *line)
{
	int	pid;

	if (*pi.exithandler == 0)
		return (0);	/* nothing configured */

	if ((pid = fork()) < 0) {
		*pi.exithandler = 0;	/* avoid coming here again. */
		printerror(1, "-ERR", "can't fork, error= %s", strerror(errno));
		}
	else if (pid == 0) {
		int	i, argc;
		char	*argv[32];

		/* setnumvar("STATUSCODE", error); -- disabled */
		setvar("MSGTEXT", line);
		for (i=0; errortype[i].num != 0; i++) {
			if (error == errortype[i].num) {
				setvar("EXITSTATUS", errortype[i].name);
				break;
				}
			}

		if (errortype[i].num == 0)	/* not found. */
			setvar("EXITSTATUS", "_UNKNOWN");

		setvar("CALLREASON", "exit-handler");
		argc = split(pi.exithandler, argv, ' ', 30);
		argv[argc] = NULL;

		execvp(argv[0], argv);
		printerror(0, "-ERR", "can't start exithandler %s, error= %s",
				argv[0], strerror(errno));
		exit (1);
		}

	return (0);
}


char *getvar(char *var)
{
	char	varname[80], *val;

	snprintf (varname, sizeof(varname) - 2, "%s%s", varprefix, var);
	if ((val = getenv(varname)) == NULL  ||  *val == 0)
		val = "-";

	return (val);
}

int setvar(char *var, char *value)
{
	char	varname[512];

  #if defined SOLARIS
	snprintf (varname, sizeof(varname) - 2, "%s%s=%s", varprefix, var, value != NULL? value: "");
	putenv(varname);
  #else
	snprintf (varname, sizeof(varname) - 2, "%s%s", varprefix, var);
	setenv(varname, value != NULL? value: "", 1);
  #endif

	return (0);
}

int setnumvar(char *var, unsigned long val)
{
	char	strval[40];

	snprintf (strval, sizeof(strval) - 2, "%lu", val);
	setvar(var, strval);

	return (0);
}


int writepidfile()
{
	if (*pi.pidfile != 0) {
		FILE	*fp;

		if ((fp = fopen(pi.pidfile, "w")) == NULL)
			printerror(1, "-ERR", "can't write pidfile: %s, error= %s", pi.pidfile, strerror(errno));

		fprintf (fp, "%d\n", getpid());
		pi.mainpid = getpid();

		fclose (fp);
		}

	return (0);
}


void exithandler(void)
{
	if (debug != 0)
		printerror(0, "+INFO", "exithandler mainpid= %u, statfile= %s", pi.mainpid, pi.statfile);

	if (pi.mainpid == getpid()) {
		if (*pi.pidfile != 0) {
			if (unlink(pi.pidfile) != 0) {
				printerror(0, "-ERR", "can't unlink pidfile %s, error= %s",
						pi.pidfile, strerror(errno));
				}
			}
		}

	if (pi.statfp != NULL) {
		rewind(pi.statfp);
		fprintf (pi.statfp, "\n");
		fclose (pi.statfp);

		if ((pi.statfp = fopen(pi.statfile, "w")) != NULL) {
			fprintf (pi.statfp, "%s", "");
			fclose (pi.statfp);
			}
		}

	if (*pi.statfile != 0) {
		if (unlink(pi.statfile) != 0) {
			printerror(0, "-ERR", "can't unlink statfile %s, error= %s",
					pi.statfile, strerror(errno));
			}
		}

	return;
}


