
/*

    File: ftpproxy/ftp.h

    Copyright (C) 1999, 2009  Wolfgang Zekoll  <wzk@quietsche-entchen.de>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef	_FTP_INCLUDED
#define	_FTP_INCLUDED


#include "ip-lib.h"	/* required for peer_t */


extern char *version;

extern char *program;
extern char progname[80];

extern int debug;
extern int extralog;
extern int bindport;
extern int daemonmode;
extern int logfacility;
extern char logname[80];


#define	FTPMAXBSIZE		4096

#define REDIR_NONE              0
#define REDIR_ACCEPT            1
#define REDIR_FORWARD           2
#define REDIR_FORWARD_ONLY      3


#ifdef FTP_FILECOPY

	/* filecopy 2.1.3: Definition of error modes */

#define	FCEM_CONTINUE		0
#define	FCEM_5XX		1
#define	FCEM_TERMINATE		2

#define	FCE_CREATEDATA		1
#define	FCE_CREATEINFO		2
#define	FCE_CREATEDIR		3
#define	FCE_CLOSEFILE		4
#define	FCE_WRITEDATA		4



	/* filecopy 4.1: Initialize filecopy, repeats for all FTP
	 * transfer commands.
	 */

#define	__init_filecopy() \
			if (x->config->cp.createcopies != 0) { \
				if (initfilecopy(x, command, x->filepath) != 0) { \
					if (x->config->cp.errormode == FCEM_5XX) { \
						cfputs(x, "500 server error"); \
						continue; \
						} \
\
					x->ch.copyfd = -1; \
					} \
				}

#endif


typedef struct _config {
    char	configfile[200];

    int		standalone;
    int		timeout;

    int 	redirmode;
    int		transparentlogin;
    int		selectserver;
    int		allow_anyremote;

    char	server[200];		/* FTP server */
    char	*serverlist;

    char	acp[200];
    char	ccp[200];
    char	ctp[200];
/*    char	varname[80]; */
    char	serverdelim[20];

    int		allow_blanks;
    int		allow_passwdblanks;

    int		use_last_at;
    int		monitor;
    int		bsize;
    char	xferlog[200];

#ifdef FTP_FILECOPY
    struct {
	int	createcopies;		/* copy mode active or not (filecopy 2.1.1) */
	char	basedir[200];		/* base directory for copy files (filecopy 2.1.2) */
	char	subdir[80];		/* strftime() format parameter to compute sub directory (filecopy 2.1.4) */
	int	errormode;		/* what to do on error: continue, 5xx, terminate? (filecopy 2.1.3) */
	} cp;
#endif

    int		numeric_only;
    char	sourceip[200];
    unsigned int dataport;
    } config_t;


#define	DIR_MAXDEPTH		15


#define	CCP_OK			0
#define	CCP_ERROR		1


#define	PORT_LISTEN		1
#define	PORT_CONNECTED		2
#define	PORT_CLOSED		3

#define	MODE_PORT		1
#define	MODE_PASSIVE		2

#define	OP_GET			1
#define	OP_PUT			2

#define	TYPE_ASC		1	/* Transfer modes for xferlog */
#define	TYPE_BIN		2

#define PIDFILE                 "/var/run/ftp.proxy.pid"

typedef struct _port {
    char	ipnum[80];
    unsigned int port;
    } port_t;

typedef struct _dtc {
    int		state;		/* LISTEN, CONNECTED, CLOSED */
    int		seen150;
    
    int		isock;
    int		osock;

#ifdef FTP_FILECOPY
    int		copyfd;		/* file handle to copy file (filecopy 2.2.4) */
#endif
    
    int		operation;	/* GET oder PUT */
    int		active;
    int		other;
    
    int		mode;		/* PORT oder PASV */
    port_t	server;
    peer_t	outside;
    peer_t	inside;
    port_t	client;

    int		type;		/* Transfer type for xferlog */
    struct timeval start1, start2;	/* Timestamps */

    char	command[20];	/* Fuer syslog Meldungen */
    char	filename[200];
    unsigned long bytes;
    } dtc_t;


typedef struct _bio {
    int		here, len;
    char	buffer[512];
    } bio_t;


typedef struct _ftp {
    config_t	*config;

    peer_t      i;		/* Proxy server's interface to client */

    int 	state;
    struct {
	char    username[200];
	char    password[80];

	char    name[200];
	char    ipnum[100];
	bio_t   bio;
	} client;

   
/*    unsigned int port; */
	
    char	username[200];		/* Username ... */
    char	password[200];		/* ... and password for server login. */
    
    struct {
	char	username[80];
	char	password[80];
	} local;

/*    struct {
 *	char	name[80];
 *	unsigned int port;
 *
 *	char	ipnum[80];
 *	} server;
 */
    peer_t		server;

    struct {
	int		server;		/* Kontrollverbindung zum Server */

	int		cfd;		/* Datenverbindung zum Client */
	int		sfd;		/* Datenverbindung zum Server */

	fd_set		fdset;
	int		max;
	} fd;

    struct {
	char    ipnum[100];
	unsigned int port;
	} origdst;

    dtc_t		ch;
    char		cwd[200];
    char		home[200];
    char		filepath[200];

    bio_t		cbuf, sbuf;
    
    char		session[80];
    int			ccpcoll;

#ifdef FTP_FILECOPY
	/* filecopy 2.2.1, 2.2.2, 2.2.3, 2.2.5, 2.2.6 */

    struct _cp {
	int		create;			/* Create copy or not */
	int		count;			/* File transfer counter */
	unsigned long	started;		/* Transfer start time */
	char		basename[200];		/* Name of transfered file without path */
	char		filename[200];		/* Name of local copy */
	char		infofile[200];		/* Name of local infofile */
	} cp;

#endif

    FILE		*xlfp;
    char		logusername[100];

    unsigned long	started;
    int			commands;
    unsigned long	btc, bts;
    } ftp_t;



extern char *getvar(char *name);
extern int setvar(char *name, char *value);
extern int setnumvar(char *name, unsigned long value);



extern int printerror(int rc, char *type, char *format, ...);
extern int acceptloop(int sock);
extern int writestatfile(ftp_t *x, char *status);
extern int getfacility(char *s);

extern int readconfig(config_t *config, char *filename, char *section);
extern int printconfig(config_t *config);

extern int proxy_request(config_t *config);

extern int setsessionvar(char *state, char *varname, char *format, ...);
extern char *getstatusline(char *string);


#ifdef FTP_FILECOPY

extern int initfilecopy(ftp_t *x, char *op, char *filename);
extern int writeinfofile(ftp_t *x, char *ftpstatus);

#endif


#endif

