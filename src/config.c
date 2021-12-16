
/*

    File: ftpproxy/config.c 

    Copyright (C) 2003, 2009  Andreas Schoenberg  <asg@ftpproxy.org> 
  
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
#include "procinfo.h"
#include "lib.h"



static int get_yesno(char **from, char *par, char *filename, int lineno)
{
	char	word[80];

	if (**from == 0) {
		printerror(1 | ERR_STDERR | ERR_CONFIG, "-ERR", "missing parameter: %s, %s:%d\n",
				par, filename, lineno);
		}

	get_word(from, word, sizeof(word));
	if (strcmp(word, "yes") == 0)
		return (1);
	else if (strcmp(word, "no") == 0)
		return (0);

	printerror(1 | ERR_STDERR | ERR_CONFIG, "-ERR", "bad parameter value: %s, parameter= %s, %s:%d\n",
			word, par, filename, lineno);

	return (0);
}

static char *get_parameter(char **from, char *par, char *value, int size,
		char *filename, int lineno)
{
	if (**from == 0) {
		printerror(1 | ERR_STDERR | ERR_CONFIG, "-ERR", "missing parameter: %s, %s:%d\n",
				par, filename, lineno);
		}

	copy_string(value, *from, size);
	if (strcmp(value, "-") == 0)
		*value = 0;

	return (value);
}

static unsigned int get_number(char **from, char *par, char *filename, int lineno)
{
	char	word[20];
	unsigned int val;

	get_parameter(from, par, word, sizeof(word), filename, lineno);
	val = strtoul(word, NULL, 0);

	return (val);
}


int readconfig(config_t *config, char *filename, char *section)
{
	int	lineno, insection, havesection, sectioncount;
	char	*p, word[80], line[300], sectname[80];
	FILE	*fp;

	if ((fp = fopen(filename, "r")) == NULL) {
		printerror(1 | ERR_STDERR | ERR_CONFIG, "-ERR", "can't open configuration file: %s\n",
				filename);
		}

	sectioncount = 0;
	if (*section == 0) {
		insection = 1;
		havesection = 1;
		}
	else {
		snprintf (sectname, sizeof(sectname) - 2, "[%s]", section);
		insection = 0;
		havesection = 0;
		}

	lineno = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		lineno++;
		if ((p = strchr(line, '#')) != NULL)
			*p = 0;

		if (*line == '[') {
			sectioncount++;
			if (*section == 0)
				break;

			p = line;
			get_word(&p, word, sizeof(word));
			if (strcmp(word, sectname) != 0)
				insection = 0;
			else {
				insection = 1;
				havesection = 1;
				}

			continue;
			}

		if (insection == 0)
			continue;

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
		else if (strcmp(word, "ctp") == 0)
			get_parameter(&p, word, config->ctp, sizeof(config->ctp), filename, lineno);

		else if (strcmp(word, "allow-anyremote") == 0)
			config->allow_anyremote = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "allow-blanks") == 0)
			config->allow_blanks = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "allow-passwdblanks") == 0)
			config->allow_passwdblanks = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "extra-logging") == 0)
			extralog = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "ipv6") == 0)
			use_ipv6 = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "monitormode") == 0)
			config->monitor = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "proxy-routing") == 0)
			config->use_last_at = get_yesno(&p, word, filename, lineno);
		else if (strcmp(word, "selectserver") == 0  ||  strcmp(word, "select-server") == 0) {
			config->selectserver = get_yesno(&p, word, filename, lineno);
			*config->server = 0;
			}

		else if (strcmp(word, "redirection") == 0) {
			char	mode[40];

			get_parameter(&p, word, mode, sizeof(mode), filename, lineno);
			if (strcmp(mode, "none") == 0  ||  strcmp(mode, "off") == 0  ||
					strcmp(mode, "no") == 0) {
				config->redirmode = REDIR_NONE;
				}
			else if (strcmp(mode, "accept") == 0  ||  strcmp(mode, "redirect") == 0)
				config->redirmode = REDIR_ACCEPT;
			else if (strcmp(mode, "forward") == 0)
				config->redirmode = REDIR_FORWARD;
			else if (strcmp(mode, "forward-only") == 0)
				config->redirmode = REDIR_FORWARD_ONLY;
			else {
				printerror(1 | ERR_STDERR | ERR_CONFIG, "-ERR", "bad redirection mode: %s, %s:%d\n",
						mode, filename, lineno);
				}
			}

		else if (strcmp(word, "server") == 0) {
			get_parameter(&p, word, config->server, sizeof(config->server), filename, lineno);
			config->selectserver = 0;
			}
		else if (strcmp(word, "serverlist") == 0)
			config->serverlist = strdup(skip_ws(p));
		else if (strcmp(word, "serverdelimiter") == 0)
			get_parameter(&p, word, config->serverdelim, sizeof(config->serverdelim), filename, lineno);
		else if (strcmp(word, "sourceip") == 0)
			get_parameter(&p, word, config->sourceip, sizeof(config->sourceip), filename, lineno);

		else if (strcmp(word, "bind") == 0) {
			bindport = get_number(&p, word, filename, lineno);
			daemonmode = 1;
			}
		else if (strcmp(word, "timeout") == 0) {
			config->timeout = get_number(&p, word, filename, lineno);
			if (config->timeout < 60)
				config->timeout = 60;
			}
		else if (strcmp(word, "xferlog") == 0)
			get_parameter(&p, word, config->xferlog, sizeof(config->xferlog), filename, lineno);

		else if (strcmp(word, "statdir") == 0) {
			char	dir[200];

			get_parameter(&p, word, dir, sizeof(dir), filename, lineno);
			setstatdir(dir);
			}

		else if (strcmp(word, "facility") == 0) {
			char	par[20];

			get_parameter(&p, word, par, sizeof(par), filename, lineno);
			logfacility = getfacility(par);
			}
		else if (strcmp(word, "logname") == 0)
			get_parameter(&p, word, logname, sizeof(logname), filename, lineno);

		else if (strcmp(word, "exithandler") == 0  ||  strcmp(word, "exit-handler") == 0) {
			char	par[400];

			get_parameter(&p, word, par, sizeof(par), filename, lineno);
			set_exithandler(par);
			}

#ifdef FTP_FILECOPY
		else if (strcmp(word, "fc.basedir") == 0)
			get_parameter(&p, word, config->cp.basedir, sizeof(config->cp.basedir), filename, lineno);
		else if (strcmp(word, "fc.subdir") == 0)
			get_parameter(&p, word, config->cp.subdir, sizeof(config->cp.subdir), filename, lineno);
		else if (strcmp(word, "fc.create-copies") == 0) {
			config->cp.createcopies = get_yesno(&p, word, filename, lineno);
			config->monitor = 1;
			}
		else if (strcmp(word, "fc.error-mode") == 0) {
			char	mode[40];

			get_parameter(&p, word, mode, sizeof(mode), filename, lineno);
			if (strcmp(mode, "continue") == 0)
				config->cp.errormode = FCEM_CONTINUE;
			else if (strcmp(mode, "terminate") == 0)
				config->cp.errormode = FCEM_TERMINATE;
			else if (strcmp(mode, "server-error") == 0  ||  strcmp(mode, "error") == 0)
				config->cp.errormode = FCEM_5XX;
			else {
				printerror(1 | ERR_STDERR | ERR_CONFIG, "-ERR", "bad redirection mode: %s, %s:%d\n",
						mode, filename, lineno);
				}
			}
#endif

		else {
			printerror(1 | ERR_STDERR | ERR_CONFIG, "-ERR", "unknown parameter: %s, %s:%d\n",
					word, filename, lineno);
			}
		}
		
	fclose (fp);
	if (*section != 0  &&  sectioncount == 0)
		havesection = 1;

	return (havesection);
}



#define	printyesno(x, y)	(y != 0? printf ("%s: %s\n", x, (y == 0)? "no": "yes"): 0)
#define printnum(x, y)		((y > 0)? printf ("%s: %u\n", x, y): 0)
#define	printstring(x, y)	((y != NULL  &&  *y != 0)? printf("%s: %s\n", x, y): 0)

int printconfig(config_t *config)
{
	char	*p;

	printf ("debug: %s\n", (debug == 0)? "no": "yes");

	printstring ("acp", config->acp);
	printyesno ("allow-anyremote", config->allow_anyremote);
	printyesno ("allow-blanks", config->allow_blanks);
	printyesno ("allow-passwdblanks", config->allow_passwdblanks);
	printnum ("bind", bindport);
	printstring ("ccp", config->ccp);
	printstring ("ctp", config->ctp);
	printstring ("exit-handler", get_exithandler());
	printyesno ("extra-logging", extralog);
	printyesno ("monitormode", config->monitor);
	p = getpidfile();  printstring ("pidfile", p);
	printyesno ("proxy-routing", config->use_last_at);
	printf ("select-server: %s\n", (config->selectserver == 0)? "no": "yes");
	printstring ("server", config->server);
	printstring ("serverlist", config->serverlist);
	p = getstatdir();  printstring ("statdir", p);
	printnum ("timeout", config->timeout);

	return (0);
}

