
/*

    File: ftpproxy/main.c

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


#ifdef FACILITY_NAMES
#define	SYSLOG_NAMES
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <signal.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/time.h>

#include "ip-lib.h"
#include "ftp.h"
#include "procinfo.h"
#include "lib.h"


#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__)
#define LOGFACILITY	LOG_FTP
#else
#define LOGFACILITY	LOG_DAEMON
#endif


char	*program =		"";
char	progname[80] =		"";

int	debug =			0;
int	extralog =		0;

int	bindport =		0;
int	daemonmode =		0;
int	logfacility =		LOGFACILITY;
char	logname[80] =		"";

int	showconfig =		0;



int setsessionvar(char *state, char *varname, char *format, ...)
{
        char    val[1024];
        va_list ap;

	if (varname != NULL  &&  *varname != 0) {
		va_start(ap, format);
		vsnprintf (val, sizeof(val) - 2, format, ap);
		va_end(ap);

		setvar(varname, val);
		}

	return (0);
}

char *getstatusline(char *status)
{
	static char line[1024];

	snprintf (line, sizeof(line) - 2, "%s, CI= { interface= %s, state= %s, client= %s, server= %s, user= %s };",
				status, getvar("INTERFACE"), getvar("SESSION_STATUS"),
				getvar("CLIENT"), getvar("SERVER"), getvar("USER"));
	return (line);
}


int printerror(int rc, char *type, char *format, ...)
{
        char    *p, tag[30], error[1024];
        va_list ap;

        va_start(ap, format);
        vsnprintf (error, sizeof(error) - 2, format, ap);
        va_end(ap);

	*tag = 0;
	if (*type != 0)
		snprintf (tag, sizeof(tag) - 2, "%s: ", type);

	p = error;
	if ((rc & ERR_EXITCODEMASK) != 0)
		p = getstatusline(error);

        if (debug != 0)
                fprintf (stderr, "%s: %s%s\n", program, tag, p);
	else if ((rc & ERR_STDERR) != 0  &&  isatty(0) != 0)
		fprintf (stderr, "%s: %s\n", program, error);
        else
                syslog(LOG_NOTICE, "%s%s", tag, p);

        if ((rc & ERR_EXITCODEMASK) != 0) {
		if (*get_exithandler() != 0)
			run_exithandler(rc & ERR_ERRORMASK, error);

		if ((rc & ERR_EXITCODEMASK) == ERR_ZEROEXITCODE)
			exit (0);

                exit (rc & ERR_EXITCODEMASK);
		}

        return (0);
}

int writestatfile(ftp_t *x, char *status)
{
	if (status != NULL  &&  *status != 0)
		setvar("SESSION_STATUS", status);

	if (getstatfp() == NULL)
		return (0);

	if (pi.statfp != NULL) {
		rewind(pi.statfp);
		fprintf (pi.statfp, "%s %s %u %lu %s:%u %s %s %s:%u %s:%u %s %s\n",
				PROXYNAME, program, getpid(),
				x->started,
				x->i.ipnum, x->i.port,
				x->client.ipnum, x->client.name,
				x->server.ipnum, x->server.port,
				x->origdst.ipnum, x->origdst.port,
				x->username,
				status);
		fflush(pi.statfp);
		}

	return (0);
}


int getfacility(char *s)
{
	int	fac;

	if (s == NULL  ||  *s == 0)
		fac = LOG_MAIL;
	else if (*s >= '0'  &&  *s <= '9') {
		fac = atoi(s);
		if (fac < 0  ||  fac >= LOG_NFACILITIES)
			fac = LOG_MAIL;
		else
			fac = LOG_MAKEPRI(atoi(s), 0);
		}
	else {
#ifdef SYSLOG_NAMES
		char	*p;
		int	i;

		for (i=0; (p = facilitynames[i].c_name) != NULL; i++) {
			if (strcasecmp(p, s) == 0)
				break;
			}

		if (p == NULL)
			fac = LOG_MAIL;
		else
			fac = facilitynames[i].c_val;
#else
		printerror(1, "-ERR", "facility names not supported");
#endif
		}

/* printerror(0, "+INFO", "s= %s, fac= %d", s, fac); */
	return (fac);
}



void signal_handler(int sig)
{
	/*
	 * Changed the way we handle broken pipes (broken control or
	 * data connection).  We ignore it here but write() returns -1
	 * and errno is set to EPIPE which is checked.
	 */

	if (sig == SIGPIPE) {
		signal(SIGPIPE, signal_handler);
		return;
		}

	printerror(1 | ERR_OTHER, "-ERR", "received signal #%d", sig);
	exit (1);
}

int set_signals(void)
{
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGPIPE, signal_handler);
	signal(SIGALRM, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	return (0);
}


void missing_arg(int c, char *string)
{
	fprintf (stderr, "%s: missing arg: -%c, %s\n", program, c, string);
	exit (1);
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
        config->allow_passwdblanks = 0;
	config->allow_anyremote = 0;
	strcpy(config->serverdelim, "@");
	config->redirmode = REDIR_NONE;
	config->transparentlogin = 0;

	setvar("NAME", "ftp.proxy");		/* Changed from putenv() -- 2007-09-25/asg */

	openlog(program, LOG_PID, logfacility);
	init_procinfo(NULL);

#ifdef FTP_FILECOPY
	strcpy(config->cp.basedir, "/var/tmp");
	config->cp.errormode = FCEM_TERMINATE;
	copy_string(config->cp.subdir, "%Y/%m/%d", sizeof(config->cp.subdir));
	/* config->monitor = 1; */
#endif


	k = 1;
	while (k < argc  &&  argv[k][0] == '-'  &&  argv[k][1] != 0) {
		copy_string(option, argv[k++], sizeof(option));
		for (i=1; (c = option[i]) != 0; i++) {
			if (c == 'd') {
				if (debug == 1)
					debug = 2;
				else
					debug = 1;
				}
			else if (c == 'a') {
				if (k >= argc)
					missing_arg(c, "access control program");

				copy_string(config->acp, argv[k++], sizeof(config->acp));
				}
			else if (c == 'B')
				config->allow_passwdblanks = 1;
			else if (c == 'b')
				config->allow_blanks = 1;
			else if (c == 'c') {
				if (k >= argc)
					missing_arg(c, "command control program");

				copy_string(config->ccp, argv[k++], sizeof(config->ccp));
				}
                        else if (c == 'C') {
				if (k >= argc)
				         missing_arg(c, "server delimeter");

				copy_string(config->serverdelim, argv[k++], sizeof(config->serverdelim));
			        }
			else if (c == 'e')
				config->selectserver = 1;
			else if (c == 'f'  ||  c == 'F') {
				if (k >= argc)
					missing_arg(c, "configuration file");

				if (c == 'F')
					showconfig = 1;

				copy_string(config->configfile, argv[k++], sizeof(config->configfile));
				}
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
			else if (c == 'q') {

				/*
				 * Specify source interface for outgoing
				 * connections -- 26JAN04asg
				 */

				if (k >= argc)
					missing_arg(c, "source interface");

				copy_string(config->sourceip, argv[k++], sizeof(config->sourceip));
				}

				/* transparent proxying 2006-02-06 */  
			else if (c == 'r'  ||  c == 'R') {
#if defined (__linux__)
				char    word[80];

				if (k >= argc)
					missing_arg(c, "redirect mode");

				if (c == 'R') {
					config->transparentlogin = 1;
					config->selectserver = 1;
					}

				copy_string(word, argv[k++], sizeof(word));
				if (strcmp(word, "none") == 0  ||  strcmp(word, "off") == 0  ||
						strcmp(word, "no") == 0) {
					config->redirmode = REDIR_NONE;
					}
				else if (strcmp(word, "accept") == 0  ||  strcmp(word, "redirect") == 0)
					config->redirmode = REDIR_ACCEPT;
				else if (strcmp(word, "forward") == 0)
					config->redirmode = REDIR_FORWARD;
				else if (strcmp(word, "forward-only") == 0)
					config->redirmode = REDIR_FORWARD_ONLY;
				else
					fprintf (stderr, "%s: bad redirect mode: %s", program, word);
#else
				fprintf (stderr, "%s: connection redirection not supported on this platform", program);
#endif
				}

			else if (c == 's') {
				if (k >= argc)
					missing_arg(c, "server list");

				config->serverlist = argv[k++];
				}
			else if (c == 't') {
				if (k >= argc)
					missing_arg(c, "timeout");

				config->timeout = atoi(argv[k++]);
				if (config->timeout < 1)
					config->timeout = 60;
				}
			else if (c == 'u')
				config->use_last_at = 1;
			else if (c == 'v') {
				if (k >= argc)
					missing_arg(c, "varname prefix");

				copy_string(varprefix, argv[k++], sizeof(varprefix));
				}
                        else if (c == 'x') {
                                if (k >= argc)
                                        missing_arg(c, "dynamic configuration program");

                                copy_string(config->ctp, argv[k++], sizeof(config->ctp));
                                }
			else if (c == 'X') {
				if (k >= argc)
					missing_arg(c, "xferlog file");

				copy_string(config->xferlog, argv[k++], sizeof(config->xferlog));
				}
			else if (c == 'y') {
				
				/*
				 * To make 'bad multihomed servers' happy and
				 * to allow server-server transfers through the
				 * proxy -- 31JAN02asg
				 */

				config->allow_anyremote = 1;
				}
 			else if (c == 'z') {
				if (k >= argc)
					missing_arg(c, "buffer size");

				config->bsize = atoi(argv[k++]);
				}
			else if (c == 'V') {
 				printf ("ftp.proxy/%s", VERSION);
#ifdef FTP_FILECOPY
				printf (" +filecopy");
#endif

#ifdef SYSLOG_NAMES
				printf (" +syslog-names");
#endif

 				printf (" asg@ftpproxy.org\n");
 				exit (0);
 				}	
			else if (c == 'D') {
				if (k >= argc)
					missing_arg(c, "port number");

				bindport = strtoul(argv[k++], NULL, 10);
				daemonmode = 1;
				}
			else if (c == 'L') {
				char	par[80];

				if (k >= argc)
					missing_arg(c, "syslog facility");

				copy_string(par, argv[k++], sizeof(par));
				if (strchr(par, ',') == NULL)
					logfacility = getfacility(par);
				else {
					char	*p, word[30];

					p = par;
					get_quoted(&p, ',', word, sizeof(word));
					logfacility = getfacility(word);
					copy_string(logname, p, sizeof(logname));
					}
				}
			else if (c == 'O') {
				if (k >= argc)
					missing_arg(c, "statdir");

				setstatdir(argv[k++]);
				}
			else if (c == 'P') {
				if (*getpidfile() == 0)
					setpidfile(PIDFILE);
				else {
					if (k >= argc)
						missing_arg(c, "pidfile");

					setpidfile(argv[k++]);
					}
				}
			else {
				fprintf (stderr, "%s: unknown option: -%c\n", program, c);
				exit (-1);
				}
			}
		}


	/*
	 * Print configuration if requested and exit
	 */

	if (showconfig != 0) {
		int	havesection = 0;
		char	*interface = "";

		readconfig(config, config->configfile, "");
		if (k < argc) {
			interface = argv[k++];
			havesection = readconfig(config, config->configfile, interface);
			}

		if (*interface == 0)
			printf ("interface: global\n");
		else {
			printf ("interface: %s\n", interface);
			printf ("status: %s\n", (havesection != 0)? "configured": "unconfigured");
			}

		printconfig(config);
		exit (0);
		}



	if (*config->configfile != 0)
		readconfig(config, config->configfile, "");


	if (logfacility != LOGFACILITY  ||  *logname != 0) {
		closelog();
		openlog(*logname == 0? program: logname, LOG_PID, logfacility);
		}


	/*
	 * Normal processing starts here.
	 */


	if (config->selectserver == 0) {

		/*
		 * Fixed proxy server together with CTP doesn't make
		 * much sense -- 040303asg
		 */

		if (*config->ctp != 0) {
			p = argv[k++];
/*			syslog(LOG_NOTICE, "configured to use ctp, ignoring server argument: %s", p); */
			fprintf (stderr, "%s: configured to use ctp, ignoring server argument: %s", program, p);
			}
		else {
			if (k >= argc) {
				fprintf (stderr, "usage: %s [<options>] <server>\n", program);
				exit (1);
				}

			copy_string(config->server, argv[k++], sizeof(config->server));
			}
		}

	if (k < argc) {
		fprintf (stderr, "%s: extra arguments on command line: %s ...\n", program, argv[k]);
		exit (1);
		}


/*
 * Moved this up, infront of the config->selectserver test -- 2007-09-25/asg
 *
 *	if (*config->configfile != 0) {
 *		readconfig(config, config->configfile, "");
 *		}
 */


	/* Just in case out varname has changed -- 2007-09-25/asg */
	setvar("NAME", "ftp.proxy");

	set_signals();
	if (daemonmode != 0) {
		signal(SIGCHLD, SIG_IGN);
		config->standalone = 1;
		if (bindport > 0) {
			int     sock;

			sock = bind_to_port("", bindport);
			writepidfile();
			setnumvar("PID", getpid());

			acceptloop(sock);
			}

		signal(SIGCHLD, SIG_DFL);
		}	

	proxy_request(config);

	exit (0);
}


